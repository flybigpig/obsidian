



## Selinux学习二


> 转自：https://www.jianshu.com/p/88a92d101532

### android上 SElinux 相关文件

+   源码：  
    system/sepolicy:  
    ├── private  
    ├── public  
    │ ├── property\_contexts  
    │ ├── property.te  
    │ ├── file\_contexts  
    │ ├── file.te  
    │ ├── su.te  
    │ ├── system\_app.te  
    │ ├── system\_server.te  
    │ ├── untrusted\_app.te  
    │ ├── priv\_app.te  
    │ └── zygote.te  
    └── vendor
    
+   ROM：  
    selinux编译生成的策略文件sepolicy，8.0之前在boot.img中，8.0由于treble的原因，system和vendor分区各放置一部分，加载的时候会进行合并。8.0之后单刷userdebug版本的boot不再能获取root权限，要刷userdebug版的system.img才行。  
    **/system/etc/selinux：**  
    plat\_file\_contexts  
    plat\_property\_contexts  
    plat\_sepolicy.cil
    
    **/vendor/etc/selinux：**  
    vendor\_file\_contexts  
    vendor\_property\_contexts  
    vendor\_sepolicy.cil
    

### my\_system\_prop定义

**property.te:**  
type my\_system\_prop, property\_type;  
property\_contexts:  
persist.my. u:object\_r:my\_system\_prop:s0

**system\_app.te:**  
set\_prop(system\_app, my\_system\_prop)

### 案例

+   模块编译selinux：make sepolicy
    
+   关闭selinux：setenforce 0
    
+   案例一：build.prop明明声明了属性，为什么通过APK和adb shell获取不到？  
    属性组成：  
    /default.prop  
    /system/build.prop  
    /vendor/default.prop  
    /vendor/build.prop
    
    由代码通过set生成的属性  
    首先，只要build.prop里声明了，就会被加载到系统属性中。属性是有权限控制的，所以APK是不能获取所有属性的。  
    参照上文my\_system\_prop的定义，如果对应APK没有声明get\_prop的权限，是获取不到相关权限的。
    
    例如：  
    persist.my.test u:object\_r:my\_system\_prop:s0  
    com.android.myapp想要去读取这个属性，但是读不到，如何分析呢？
    
    查看属性的安全上下文：getprop -Z persist.my.test  
    \[persist.my.test\]: \[u:object\_r:my\_system\_prop:s0\]
    
    查看进程的安全上下文：ps -AZ | grep com.android.myapp  
    u:r:platform\_app:s0:c512,c768 com.android.myapp  
    给platform\_app加权限，在platform\_app.te中添加：  
    get\_prop(platform\_app, my\_system\_prop)  
    adb shell对应的身份是shell，也是受限的，只有adb root后getprop获取的属性才是最全的。
    
+   案例二：新增allow xxx权限，编译报错Neverallow，如何处理？
    
    cts版本不能有任何neverallow，只能去掉添加的权限  
    国内版本可适当注释掉原生相关neverallow进行规避  
    neverallow check failed at out/target/product/sailfish/obj/ETC/plat\_sepolicy.cil\_intermediates/plat\_sepolicy.cil:6373 from system/sepolicy/public/domain.te:1133  
    (neverallow base\_typeattr\_144 file\_type (file (execmod)))
    
    allow at out/target/product/sailfish/obj/ETC/vendor\_sepolicy.cil\_intermediates/vendor\_sepolicy.cil:1396  
    (allow platform\_app\_28\_0 app\_data\_file\_28\_0 (file (execute execmod)))  
    如上看出，具体是public/domain.te:1133的限制影响了新加权限，找到对应行数观察:
    
    neverallow { domain -untrusted\_app\_all } file\_type:file execmod;  
    解决方式：
    
    直接注释到该行
    
    ```shell
    # neverallow { domain -untrusted_app_all } file_type:file execmod;
    ```
    

```
	只规避受影响的platform_app
	
	```
	neverallow { domain -untrusted_app_all -platform_app } file_type:file execmod;
```

+   案例三：avc: denied { write }之类的缺少权限如何处理？

audit(0.0:67): avc: denied { write } for path="/dev/block/vold/93:96" dev=“tmpfs” ino=/1263 scontext=u:r:kernel:s0 tcontext=u:object\_r:block\_device:s0 tclass=blk\_file permissive=0

语法：rule\_name source\_type target\_type : class perm\_set\*\*  
万能公式：  
缺少什么权限：{ write }权限  
谁缺少权限：scontext=u:r:kernel:s0  
对谁缺少权限：tcontext=u:object\_r:block\_device:s0  
什么类型：tclass=blk\_file

kernel.te:  
allow kernel block\_device:blk\_file write;  
写操作一般还伴随open、append等，所以一般使用w\_file\_perms宏替代单一的write

+   案例四：avc: denied { execmod }如何处理？

audit(0.0:51): avc: denied { execmod } for path="/system/app/education\_student/lib/arm/libhpHandPends.so" dev=“mmcblk0p24” ino=424 scontext=u:r:untrusted\_app:s0:c512,c768 tcontext=u:object\_r:system\_file:s0 tclass=file permissive=0

按照万能公式可得如下策略语句，这是没问题的。但这不是最优解，也可能违反Neverallow。

allow untrusted\_app system\_file:file execmod;

目前android加载so的策略是强制使用地址无关代码的模式，execmod的本质是由于so不支持地址无关导致加载失败。  
地址无关可以在多个进程间共享so的代码指令，无需拷贝重定位，节省内存。

目前的Android.mk、NDK都默认加有-fPIC，只有很早之前的编译的so存在此问题。  
因此最优解是重新编译so，GCC编译时加上-fPIC参数即可。