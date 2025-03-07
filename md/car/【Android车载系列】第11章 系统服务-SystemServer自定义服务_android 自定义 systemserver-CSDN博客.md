## 1 编写自定义系统服务

### 1.1 [AIDL](https://so.csdn.net/so/search?q=AIDL&spm=1001.2101.3001.7020)接口定义

系统源码目录`/frameworks/base/core/java/android/app/`下新建AIDL接口IYvanManager.aidl

```
package android.app;

/**
* 目录：/frameworks/base/core/java/android/app/IYvanManager.aidl
*/
interface IYvanManager{
    String request(String msg);
}

```

### 1.2 服务端

```
package com.android.server.yvan;

import android.app.IYvanManager;
import android.os.RemoteException;


/**
 * 服务端   AMS PMS WMS
 * 目录：/frameworks/base/services/core/java/com/android/server/yvan/YvanManagerService.java
 */
public class YvanManagerService extends IYvanManager.Stub {
    @Override
    public String request(String msg) throws RemoteException {
        return "YvanManagerService接收数据:"+msg;
    }
}
```

### 1.3 客户端

```
package android.app;

import android.annotation.SystemService;
import android.compat.annotation.UnsupportedAppUsage;
import android.content.Context;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Singleton;
import android.os.ServiceManager;
import android.annotation.Nullable;
/**
 * 客户端
 * 目录：/frameworks/base/core/java/android/app/YvanManager.java
 */
@SystemService(Context.YVAN_SERVICE)
public class YvanManager{
    /**
     * @hide
     */
    public YvanManager() {
    }
    /**
     * @hide
     */
    public static IYvanManager getServerice(){
        return I_YVAN_MANAGER_SINGLETON.get();
    }

    @UnsupportedAppUsage
    private static final Singleton<IYvanManager> I_YVAN_MANAGER_SINGLETON =
            new Singleton<IYvanManager>() {
                @Override
                protected IYvanManager create() {
                    final IBinder b= ServiceManager.getService(Context.YVAN_SERVICE);
                    final IYvanManager im=IYvanManager.Stub.asInterface(b);
                    return im;
                }
            };


    @Nullable
    public String request(@Nullable String msg){
        try{
            return getServerice().request(msg);
        }catch (RemoteException e){
            throw e.rethrowFromSystemServer();
        }
    }
}
```

### 1.4 添加上下文件常量

1.  `/frameworks/base/core/java/android/context/Context.java`文件下添加服务名字：

```
public static final String YVAN_SERVICE = "yvan";
```

2.  常量内增加该服务`YVAN_SERVICE`

```
@StringDef(suffix = { "_SERVICE" }, value = {
            POWER_SERVICE,
            //@hide: POWER_STATS_SERVICE,
            WINDOW_SERVICE,
            LAYOUT_INFLATER_SERVICE,
            ACCOUNT_SERVICE,
            ACTIVITY_SERVICE,
            YVAN_SERVICE,
```

### 1.5 注册BINDER

#### 1.5.1 `frameworks/base/services/java/com/android/server/SystemServer.java`文件下`startOtherServices()`方法下将服务添加

```
ServiceManager.addService(Context.YVAN_SERVICE,new YvanManagerService());
```

#### 1.5.2 获取服务在Context中`getSystemService()`方法ContextImpl中实现

```
SystemServiceRegistry.getSystemService(this, name);
```

#### 1.5.3 `frameworks/base/core/java/android/app/SystemServiceRegistry.java`为用于给客户端获取服务的类，其中有一个static块 执行了`registerService()`用于注册

```
registerService(Context.YVAN_SERVICE, YvanManager.class,
                new CachedServiceFetcher<YvanManager>() {
                    @Override
                    public YvanManager createService(ContextImpl ctx) {
                        return new YvanManager();
                    }});
```

#### 1.5.4 最后修改SeLinux安全权限

`system/sepolicy/prebuilts/api/32.0/private/ 与 system/sepolicy/private/` 目录下，分别修改以下三个文件  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/94aa02a249b473ad1693f8b57bac1723.png)

> 1.service\_contexts  
> #配置自定义服务selinux角色  
> yvan u:object\_r:yvan\_service:s0  
> 用户:角色:类型:安全级别

> 2.service.te  
> #配置自定义服务类型的权限  
> type yvan\_service,  
> app\_api\_service, ephemeral\_app\_api\_service,  
> system\_server\_service,  
> service\_manager\_type;

> 3.untrusted\_app\_all.te  
> #允许所有app使用自定义服务  
> allow untrusted\_app\_all yvan\_service:service\_manager find;

#### 1.5.5 更新并编译

#更新：make update-api  
#编译：m  
#运行[模拟器](https://so.csdn.net/so/search?q=%E6%A8%A1%E6%8B%9F%E5%99%A8&spm=1001.2101.3001.7020)：emulator

#### 1.5.6 服务添加成功验证

```
adb shell service list |grep yvan
```

## 2 使用自定义服务

### 2.1 方法一：利用双亲委托机制

一般只是用来调试自己的服务功能是否正常

```
package android.app;

public class YvanManager {
    public String request(String msg){
        return null;
    }
}

// 使用
YvanManager yvanManager=(YvanManager)getSystemService("yvan");
String str=yvanManager.request("app msg!");
Log.i("yvan",str);
```

### 2.2 方法二：通过修改SDK配置

自定义SDK给应用层使用

#### 2.2.1 把正在使用的SDK复制一份并改名为android-32.car，android-32.car中的platforms和sources下的平台同样也复制一份

#### 2.2.2 将复制出来的原生SDK/platforms中的android.jar用自己编译出的替换

out/target/common/obj/JAVA\_LIBRARIES/android\_stubs\_current\_intermediates/classes-header.jar

#### 2.2.3 修改SDK配置

##### 2.2.3.1 修改android-32.car\\platforms\\android-321\\source.properties

```
#指定自定义平台标识为321（可以是任意数字，但为了与原生标识区分，请使用三位数） 
#修改： 
Pkg.Desc=Android SDK Platform 321 
Pkg.UserSrc=false 
#修改： 
Platform.Version=321 
Platform.CodeName= 
Pkg.Revision=1 
#修改： 
AndroidVersion.ApiLevel=321 
Layoutlib.Api=15 
Layoutlib.Revision=1 
Platform.MinToolsRev=22
```

##### 2.2.3.2 修改 android-32.car\\platforms\\android-321\\package.xml

```
<localPackage path="platforms;android-321" obsolete="false"> 
 <type-details xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
xsi:type="ns5:platformDetailsType"> 
 <!-- 修改 --> 
 <api-level>321</api-level> 
 <codename></codename> 
 <layoutlib api="15"/></type-details> 
 <revision> 
 <major>1</major> 
 </revision> 
 <!-- 修改 --> 
 <display-name>Android SDK Platform 321</display-name> 
 <uses-license ref="android-sdk-license"/> 
</localPackage>
```

#### 2.2.4 配置源码跳转

修改`android-32.car\sources\android-321`目录下的，参考第3步`source.properties`和`package.xml`文件

## 3 思考题

如何编写自定义服务并要求有系统执行过程的`生命周期管理`？？？参考[AMS](https://so.csdn.net/so/search?q=AMS&spm=1001.2101.3001.7020)服务的生命周期管理。