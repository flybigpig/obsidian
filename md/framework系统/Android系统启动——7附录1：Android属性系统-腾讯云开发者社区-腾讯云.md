本篇文章的主要内容如下：

-   1、Android 属性系统介绍
-   2、Android的属性系统与Linux环境变量
-   3、Android 属性系统的创建
-   4、Android 属性系统的初始化
-   5、启动属性服务

### 一、Android 属性系统介绍

##### (一)、介绍

> Android 系统的属性系统(Property)系统有点类似于Window的注册表，其中的每个属性被构造成键值对(key/value)供外界使用。

简单的来说Android的属性系统可以简单的总结为以下几点：

-   Android系统一启动就会从若干属性脚本文件中加载属性内容
-   Android系统中的所有属性(key/value)会存入同一块共享内存中
-   系统中的各个进程会将这块共享内存映射到自己的内存空间，这样就可以直接读取属性内容了
-   系统中只有一个实体可以设置、修改属性值，它就是属性系统(init进程)
-   不同进程只可以通过sockeet方式，向属性系统(init进程)发出修改，而不能直接修改属性值
-   共享内存中的键值内容会以一种字典树的形式进行组织。

下图是属性系统的演示

![](https://ask.qcloudimg.com/http-save/yehe-2957818/oftfxgn0kz.png)

属性系统.png

##### (二)、举例

属性系统在Android 系统中大量使用，用来保存系统级别的设置或者在进程间传递一些简单的信息。每个属性由属性名称和属性值组成，名称通常是一串‘.’分割的字符串，这些名称的前缀有特定的含义，不能随意改动，但是前缀后面的字符串可以由应用程序来制定。而且属性值只能是字符串，如果需要在程序中使用数值，需要自定完成字符串和数值之间的转换。

因为Android的 属性值既可以在Java层调用，所以`Java`层获取和设置属性的方法原型如下：

```
public static String getProperty(String key,  String defaultValue);
public static String setProperty(String key,  String vlaue);
```

对应的`Native`层获取和设置属性的函数原型如下：

```
int property_get(const char *key ,  char *value , const char *default_value)
int property_set(const char *key ,  char *value)
```

系统中每个进程都可以调用这些函数来读取和修改属性。读取属性值对任何进程都是没有限制的，直接由本进程从共享区域读取；但是修改属性值必须通过init进程来完成，这样init进程就可以检查请求的进程是否有相应的权限来修改该属性值。如果属性值修改成功后，init进程会检查init.rc文件是否已经定义了和该属性值匹配的"触发器(trigger)"。如果有定义，则执行"触发器"下的命令。

举一个触发器的例子

```
on property:ro.dubuggeable=1
     start console
```

这个"触发器"的含义是：一旦属性ro.debuggable被设置为"1"，则执行命令"start console"，启动console进程。

### 二、Android的属性系统与Linux环境变量

Android的属性系统表面上看和Linux的环境变量很类似，都是以字符串的形式保存系统键值提供给进程间信息使用。大家很容易弄混他们的区别，以为他们是一样的，其实他们在Android系统内部是同时存在的。

##### (一) Android的属性系统

我们怎么才能查看到Android系统的所有属性值，其实很简答

-   首先 确保，你本地有手机相连接；如果没有手机，请打开模拟器
-   其次 找到Android Studio的`Terminal`，输入**adb shell**
-   最后 进入adb后，输入**getprop**

下图是我的模拟器上的**属性值**

![](https://ask.qcloudimg.com/http-save/yehe-2957818/o9adv7v7eb.png)

Android的属性系统.png

##### (二) Android的系统环境变量

那我们怎么才能查看Android系统的环境变量呢，其实和上面差不多

-   首先 确保，你本地有手机相连接；如果没有手机，请打开模拟器
-   其次 找到Android Studio的`Terminal`，输入**adb shell**
-   最后 进入adb后，输入**export -p**

下图是我的模拟器上的**环境变量**

![](https://ask.qcloudimg.com/http-save/yehe-2957818/agzn9o7f1m.png)

Android的系统环境变量.png

> 属性系统和环境变量相比，环境变量的使用比较随意，缺乏控制；而属性系统对名称的定义以及修改的权限都增加了限制，增强了安全性，更适合用于程序的配置管理。

### 三、Android 属性系统的创建

> Android 属性系统 的启动是在init进程里面启动的，前面讲解了，init进程是Android 中Linux里面的第一个进程。

我们知道init启动在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)的`main()`方法里面，按我们就来看下[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)的`main`函数，如下：

```
989 int main(int argc, char** argv) {
        ...
1030        property_init();
        ...
1137    return 0;
1138}
```

我们看到了在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)的`main`函数里面调用了`property_init()`函数，而`property_init()`的实现是在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)里面，那我们就来看一下

代码在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 74行

```
74void property_init() {
75    if (property_area_initialized) {
76        return;
77    }
78
79    property_area_initialized = true;
80
81    if (__system_property_area_init()) {
82        return;
83    }
84
//++++++++++++++++++++++++ 分割线
85    pa_workspace.size = 0;
86    pa_workspace.fd = open(PROP_FILENAME, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
87    if (pa_workspace.fd == -1) {
88        ERROR("Failed to open %s: %s\n", PROP_FILENAME, strerror(errno));
89        return;
90    }
91}
```

> PS: 关于86行`open`函数入参的解释

-   O\_RDWR：读写
-   O\_CREAT：若不存在，则创建
-   O\_NOFOLLOW：如果filename是软连接，则打开失败
-   O\_EXCL：如果使用O\_CREAT是文件存在，则可返回错误信息

我将这个方法的内容主要分为两个部分：

-   上半部：创建和初始化属性的共享内存空间： 我们看到里面调用`property_area_initialized`来判断是否初始化过，如果初始化完毕，则直接返回；没有经过初始化则调用`__system_property_area_init()`函数来进行初始化。
-   下半部：初始化workspace对象： 然后调用open函数，来给pa\_workspace.fd赋值。

下半部分没什么好讲解的，那我们就来看下上半部分的内容

##### 创建和初始化属性的共享内存空间：

`__system_property_area_init()`函数这个函数的具体实现是在**bionic**中的[system\_properties.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fbionic%252Flibc%252Fbionic%252Fsystem_properties.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)，下面我们就来看下

代码在[system\_properties.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fbionic%252Flibc%252Fbionic%252Fsystem_properties.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 796行

```
596int __system_property_area_init()
597{
598    return map_prop_area_rw();
599}
```

我们看到好像什么也没做，就是直接调用`map_prop_area_rw()`函数，那我们就来看一下这个`map_prop_area_rw()`函数的内容

代码在[system\_properties.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fbionic%252Flibc%252Fbionic%252Fsystem_properties.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 185行

```
185static int map_prop_area_rw()
186{
187    /* dev is a tmpfs that we can use to carve a shared workspace
188     * out of, so let's do that...
189     */
//************************* 第1部分 ********************
190    const int fd = open(property_filename,
191                        O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_EXCL, 0444);
192
//************************* 第2部分 ********************
193    if (fd < 0) {
194        if (errno == EACCES) {
195            /* for consistency with the case where the process has already
196             * mapped the page in and segfaults when trying to write to it
197             */
198            abort();
199        }
200        return -1;
201    }
202
203    if (ftruncate(fd, PA_SIZE) < 0) {
204        close(fd);
205        return -1;
206    }
207
       //设置内存映射区表的长度,128kb 
208    pa_size = PA_SIZE;
       //数据大小设置
209    pa_data_size = pa_size - sizeof(prop_area);
210    compat_mode = false;
211
//************************* 第3部分 ********************
       // 将 /dev/__properties__ 设备文件映射到内存中，可读写
       // 其中 MAP_SHARED：表示对映射区域的写入数据会复制回文件内，与其它所有映射这个文件的进程共享映射空间
212    void *const memory_area = mmap(NULL, pa_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
213    if (memory_area == MAP_FAILED) {
214        close(fd);
215        return -1;
216    }
217
       //设置属性共享区域的标志和版本号,此时共享区域没有任何数据;  
218    prop_area *pa = new(memory_area) prop_area(PROP_AREA_MAGIC, PROP_AREA_VERSION);
219
//************************* 第4部分 ********************
220    /* plug into the lib property services */
221    __system_property_area__ = pa;
222
223    close(fd);
224    return 0;
225}
```

我将上面的代码分为4个部分

-   **第1部分**：打开属性区域用于共享的设备文件"/dev/\_properties"。普通进程只需要读取属性值，因此，这里以只读的方式打开设备文件。 `O_CLOEXEC`：的作用是使进程fork出的子进程自动关闭这个fd。 -**第2部分**：这里做一些准备工作，比如如果打不开，判断其原因等 -**第3部分**：执行mmap来获得共享区域的指针，并判断是否失败 -**第4部分**：最后将属性共享区的指针pa保存到全局变量`__system_property_area__`中。这样当需要读取某个属性时，可以直接使用这个全局变量。

其实就是系统把/dev/**properties** 设备文件映射到共享内存中，并会在该内存起始位置设置共享区域的标志和版本号；最后，会以只读的方式再打开一次/dev/**properties** 设备文件，并将它的fd保存在workspace 结构的对象中。在service\_start()函数中，会将该fd发布到系统中。

> 至此，关于属性系统共享空间已经创建完毕。创建完共享区域后，接下来就需要初始化系统已有的属性值，那属性系统是什么时候初始化的呢？就让我们来看下

### 四、Android 属性系统的初始化

Android属性系统的初始化的根源也是[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)的`main`函数里面，通过`property_load_boot_defaults()`函数来加载默认的属性

```
989 int main(int argc, char** argv) {
        ...
1077    property_load_boot_defaults();
        ...
1137    return 0;
1138}
```

`property_load_boot_defaults`函数是在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)里面，那我们就来看下。

代码在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 494行

```
494void property_load_boot_defaults() {
495    load_properties_from_file(PROP_PATH_RAMDISK_DEFAULT, NULL);
496}
```

我们看到`property_load_boot_defaults`函数里面什么都没有做，就是调用`load_properties_from_file`函数。而`load_properties_from_file`函数是从**宏**PROP\_PATH\_RAMDISK\_DEFAULT表示的文件中读取属性值，并设置到对应地方属性中

在这里涉及几个常量，为了大家更好的理解，我直接显示器结果，如下：

-   **PROP\_PATH\_RAMDISK\_DEFAULT**："/default.prop"
-   **PROP\_PATH\_SYSTEM\_BUILD**："/system/build.prop"
-   **PROP\_PATH\_VENDOR\_BUILD**："/vendor/build.prop"
-   **PROP\_PATH\_LOCAL\_OVERRIDE**："/data/local.prop"
-   **PROP\_PATH\_FACTORY**："/factory/factory.prop"

首选我们来看下`load_properties_from_file`的具体实现，也在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)

```
422/*
423 * Filter is used to decide which properties to load: NULL loads all keys,
424 * "ro.foo.*" is a prefix match, and "ro.foo.bar" is an exact match.
425 */
426static void load_properties_from_file(const char* filename, const char* filter) {
427    Timer t;
428    std::string data;
429    if (read_file(filename, &data)) {
430        data.push_back('\n');
431        load_properties(&data[0], filter);
432    }
433    NOTICE("(Loading properties from %s took %.2fs.)\n", filename, t.duration());
434}
```

我们看到在`load_properties_from_file`函数中我们看到，它首先读取文件filename，然后调用`load_properties`函数来装载属性值，那我们就研究下`load_properties`函数

代码 [property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 366行

```
362/*
363 * Filter is used to decide which properties to load: NULL loads all keys,
364 * "ro.foo.*" is a prefix match, and "ro.foo.bar" is an exact match.
365 */
366static void load_properties(char *data, const char *filter)
367{
368    char *key, *value, *eol, *sol, *tmp, *fn;
369    size_t flen = 0;
370
371    if (filter) {
372        flen = strlen(filter);
373    }
374
375    sol = data;
376    while ((eol = strchr(sol, '\n'))) {
377        key = sol;
378        *eol++ = 0;
379        sol = eol;
380
381        while (isspace(*key)) key++;
382        if (*key == '#') continue;
383
384        tmp = eol - 2;
385        while ((tmp > key) && isspace(*tmp)) *tmp-- = 0;
386
387        if (!strncmp(key, "import ", 7) && flen == 0) {
388            fn = key + 7;
389            while (isspace(*fn)) fn++;
390
391            key = strchr(fn, ' ');
392            if (key) {
393                *key++ = 0;
394                while (isspace(*key)) key++;
395            }
396
397            load_properties_from_file(fn, key);
398
399        } else {
400            value = strchr(key, '=');
401            if (!value) continue;
402            *value++ = 0;
403
404            tmp = value - 2;
405            while ((tmp > key) && isspace(*tmp)) *tmp-- = 0;
406
407            while (isspace(*value)) value++;
408
409            if (flen > 0) {
410                if (filter[flen - 1] == '*') {
411                    if (strncmp(key, filter, flen - 1)) continue;
412                } else {
413                    if (strcmp(key, filter)) continue;
414                }
415            }
416
417            property_set(key, value);
418        }
419    }
420}
421
```

这里的主要内容就是把这些属性发布到系统中。至此实现了，从文件中导入默认的系统属性。

### 五、启动属性服务

属性服务同样也是在init中启动的，代码也是在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)的`main`函数里面。

```
989 int main(int argc, char** argv) {
        ...
1078    start_property_service();
        ...
1137    return 0;
1138}
```

是通过`start_property_service()`函数来启动属性服务的。那我们就来看下`start_property_service()`函数的具体实现，这个函数同样也是在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined)里面

代码在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 570行

```
570void start_property_service() {
571    property_set_fd = create_socket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
572                                    0666, 0, 0, NULL);
573    if (property_set_fd == -1) {
574        ERROR("start_property_service socket creation failed: %s\n", strerror(errno));
575        exit(1);
576    }
577
578    listen(property_set_fd, 8);
579
580    register_epoll_handler(property_set_fd, handle_property_set_fd);
581}
```

我们看到`start_property_service`函数创建了socket，然后监听，并且调用`register_epoll_handler`函数把socket的fd放入epoll中。

我们先来先看`create_socket`的具体实现

##### 1、create\_socket函数解析

代码在[util.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Futil.cpp%252391&objectId=1199514&objectType=1&isNewArticle=undefined)

```
85/*
86 * create_socket - creates a Unix domain socket in ANDROID_SOCKET_DIR
87 * ("/dev/socket") as dictated in init.rc. This socket is inherited by the
88 * daemon. We communicate the file descriptor's value via the environment
89 * variable ANDROID_SOCKET_ENV_PREFIX<name> ("ANDROID_SOCKET_foo").
90 */
91int create_socket(const char *name, int type, mode_t perm, uid_t uid,
92                  gid_t gid, const char *socketcon)
93{
94    struct sockaddr_un addr;
95    int fd, ret;
96    char *filecon;
97
98    if (socketcon)
99        setsockcreatecon(socketcon);
100
101    fd = socket(PF_UNIX, type, 0);
102    if (fd < 0) {
103        ERROR("Failed to open socket '%s': %s\n", name, strerror(errno));
104        return -1;
105    }
106
107    if (socketcon)
108        setsockcreatecon(NULL);
109
110    memset(&addr, 0 , sizeof(addr));
111    addr.sun_family = AF_UNIX;
       // ANDROID_SOCKET_DIR：:/dev/socket     这里传入的name为property_service
       //  这里为socket设置了设备文件：/dev/socket/property_service
112    snprintf(addr.sun_path, sizeof(addr.sun_path), ANDROID_SOCKET_DIR"/%s",
113             name);
114
       // 删掉之前的设备文件
115    ret = unlink(addr.sun_path);
116    if (ret != 0 && errno != ENOENT) {
117        ERROR("Failed to unlink old socket '%s': %s\n", name, strerror(errno));
118        goto out_close;
119    }
120
121    filecon = NULL;
122    if (sehandle) {
123        ret = selabel_lookup(sehandle, &filecon, addr.sun_path, S_IFSOCK);
124        if (ret == 0)
125            setfscreatecon(filecon);
126    }
127
       // 将addr 与socket绑定起来
128    ret = bind(fd, (struct sockaddr *) &addr, sizeof (addr));
129    if (ret) {
130        ERROR("Failed to bind socket '%s': %s\n", name, strerror(errno));
131        goto out_unlink;
132    }
133
134    setfscreatecon(NULL);
135    freecon(filecon);
136
        // 设置权限
137    chown(addr.sun_path, uid, gid);
138    chmod(addr.sun_path, perm);
139
140    INFO("Created socket '%s' with mode '%o', user '%d', group '%d'\n",
141         addr.sun_path, perm, uid, gid);
142
143    return fd;
144
145out_unlink:
146    unlink(addr.sun_path);
147out_close:
148    close(fd);
149    return -1;
150}
```

我们看到这里是创建了一个socket，并将该socket与"/dev/socket/property\_service"这个设备文件进行绑定。这一点很重要，因为我们在设置属性时，会首先拿到该文件的fd，向里面写数据；这时epoll会检测到该socket可读，从而调用注册的事件处理函数来进行处理；然后在socket进行监听，等待对该socket的连接请求。最后会向epoll\_fd注册这个socket，同时也注册了事件处理函数handle\_property\_set\_fd。

下面让我们来看下`register_epoll_handler`函数内部的处理逻辑

##### 2、register\_epoll\_handler函数解析

代码在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 87行

```
87void register_epoll_handler(int fd, void (*fn)()) {
88    epoll_event ev;
      // 文件描述符可读
89    ev.events = EPOLLIN;
      // 保存指定的函数指针，用于后续的事件处理
90    ev.data.ptr = reinterpret_cast<void*>(fn);
91    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {  
         // 向epoll_fd添加要监听的fd，比如property，keychord和signal事件的监听
92        ERROR("epoll_ctl failed: %s\n", strerror(errno));
93    }
94}
```

所以当eoll轮训发现此socket有数据到来，即有属性设置请求时，会调用`handle_property_set_fd()`函数去处理该事件。那我们就来看下事件处理函数handle\_property\_set\_fd

##### 3、handle\_property\_set\_fd函数解析

代码在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp%2523property_set_fd&objectId=1199514&objectType=1&isNewArticle=undefined) 262行

```
262static void handle_property_set_fd()
263{
264    prop_msg msg;
265    int s;
266    int r;
267    struct ucred cr;
268    struct sockaddr_un addr;
269    socklen_t addr_size = sizeof(addr);
270    socklen_t cr_size = sizeof(cr);
271    char * source_ctx = NULL;
272    struct pollfd ufds[1];
273    const int timeout_ms = 2 * 1000;  /* Default 2 sec timeout for caller to send property. */
274    int nr;
275
       // 等待、处理客户端的socket连接请求
276    if ((s = accept(property_set_fd, (struct sockaddr *) &addr, &addr_size)) < 0) {
277        return;
278    }
279
280    /* Check socket options here */
        // 通过设置SO_PEERCRED返回连接到此套接字的进程的凭据，用于检测对客户端进程的身份
281    if (getsoc kopt(s, SOL_SOCKET, SO_PEERCRED, &cr, &cr_size) < 0) {
282        close(s);
283        ERROR("Unable to receive socket options\n");
284        return;
285    }
286
287    ufds[0].fd = s;
288    ufds[0].events = POLLIN;
289    ufds[0].revents = 0;
       // 等待客户端socket发送数据
290    nr = TEMP_FAILURE_RETRY(poll(ufds, 1, timeout_ms));
291    if (nr == 0) {
292        ERROR("sys_prop: timeout waiting for uid=%d to send property message.\n", cr.uid);
293        close(s);
294        return;
295    } else if (nr < 0) {
296        ERROR("sys_prop: error waiting for uid=%d to send property message: %s\n", cr.uid, strerror(errno));
297        close(s);
298        return;
299    }
300 
       // 获取客户端发送的属性设置数据
301    r = TEMP_FAILURE_RETRY(recv(s, &msg, sizeof(msg), MSG_DONTWAIT));
       // 判断数据的大小是否合法
302    if(r != sizeof(prop_msg)) {
303        ERROR("sys_prop: mis-match msg size received: %d expected: %zu: %s\n",
304              r, sizeof(prop_msg), strerror(errno));
305        close(s);
306        return;
307    }
308
       // 识别操作码
309    switch(msg.cmd) {
310    case PROP_MSG_SETPROP:
311        msg.name[PROP_NAME_MAX-1] = 0;
312        msg.value[PROP_VALUE_MAX-1] = 0;
313
           // 判断是否合法
314        if (!is_legal_property_name(msg.name, strlen(msg.name))) {
315            ERROR("sys_prop: illegal property name. Got: \"%s\"\n", msg.name);
316            close(s);
317            return;
318        }
319
320        getpeercon(s, &source_ctx);
321
322        if(memcmp(msg.name,"ctl.",4) == 0) {
323            // Keep the old close-socket-early behavior when handling
324            // ctl.* properties.
325            close(s);
326            if (check_control_mac_perms(msg.value, source_ctx)) {
327                handle_control_message((char*) msg.name + 4, (char*) msg.value);
328            } else {
329                ERROR("sys_prop: Unable to %s service ctl [%s] uid:%d gid:%d pid:%d\n",
330                        msg.name + 4, msg.value, cr.uid, cr.gid, cr.pid);
331            }
332        } else {
333            if (check_perms(msg.name, source_ctx)) {
334                property_set((char*) msg.name, (char*) msg.value);
335            } else {
336                ERROR("sys_prop: permission denied uid:%d  name:%s\n",
337                      cr.uid, msg.name);
338            }
339
340            // Note: bionic's property client code assumes that the
341            // property server will not close the socket until *AFTER*
342            // the property is written to memory.
343            close(s);
344        }
345        freecon(source_ctx);
346        break;
347
348    default:
349        close(s);
350        break;
351    }
352}
353
```

首先，会等待客户端的连接请求，当有客户端请求连接时，还会去获取该客户端进程的ucred结构信息(里面有pid,uid,gid)。最后等待客户端发送数据，并同时准备接受这些数据；数据收到后，先判断PROP\_MSG\_SETPROP操作码，这是客户端封装数据时设置的。接着通过`is_legal_property_name`函数检测请求的属性名称是否合法的。检测完属性名称后，如果此时使用的控制类指令"ctl."，则先关闭此次socket，然后检测客户端socket的进程是否有权限设置控制类属性；最后调用`handle_control_message`函数处理该请求。

那我们来看下`is_legal_property_name`函数的具体实现，

##### 3.1 is\_legal\_property\_name函数解析

代码如下： 代码在[property\_service.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Fproperty_service.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 176行

```
175static bool is_legal_property_name(const char* name, size_t namelen)
176{
177    size_t i;
178    if (namelen >= PROP_NAME_MAX) return false;
179    if (namelen < 1) return false;
180    if (name[0] == '.') return false;
181    if (name[namelen - 1] == '.') return false;
182
183    /* Only allow alphanumeric, plus '.', '-', or '_' */
184    /* Don't allow ".." to appear in a property name */
185    for (i = 0; i < namelen; i++) {
186        if (name[i] == '.') {
187            // i=0 is guaranteed to never have a dot. See above.
188            if (name[i-1] == '.') return false;
189            continue;
190        }
191        if (name[i] == '_' || name[i] == '-') continue;
192        if (name[i] >= 'a' && name[i] <= 'z') continue;
193        if (name[i] >= 'A' && name[i] <= 'Z') continue;
194        if (name[i] >= '0' && name[i] <= '9') continue;
195        return false;
196    }
197
198    return true;
199}
```

这里检测的依据有：

-   属性名称的长度必须大于等于1，小于32
-   属性名称不能以"."开头和结尾
-   属性名称不能出现连续的"."
-   属性的名称必须以"."为分隔符，且只能使用：'0'-'9'、'a'-'z'、'A'-'Z'、'-'及'\_'等字符

##### 3.2 handle\_control\_message函数解析

代码在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Finit%252Finit.cpp&objectId=1199514&objectType=1&isNewArticle=undefined) 530行

```
530void handle_control_message(const char *msg, const char *arg)
531{
532    if (!strcmp(msg,"start")) {
533        msg_start(arg);
534    } else if (!strcmp(msg,"stop")) {
535        msg_stop(arg);
536    } else if (!strcmp(msg,"restart")) {
537        msg_restart(arg);
538    } else {
539        ERROR("unknown control msg '%s'\n", msg);
540    }
541}
```

就是根据其传入的参数，执行器相应的方法，比如`msg_start`方法

### 官人\[飞吻\]，你都把臣妾从头看到尾了，喜欢就点个赞呗(眉眼)！！！

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2018.03.07 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除