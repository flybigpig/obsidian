## \[Android 13\]自编译ROM网络连接受限

![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[坂田民工](https://blog.csdn.net/qq_40731414 "坂田民工") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2023-06-05 19:04:33 修改

于 2023-06-05 19:03:11 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

Android设备在刷了自己编译的固件后，通常连接 WiFi 会出现 “网络连接受限” 这样的字样，原因就是google默认的网络探针地址是国外的，我们在国内无法正常访问，所以出现网络受限的现象，因此我们可以通过下面的方法修改这个探针来解决这个问题。

方法一 (属性常量修改,可用于调试)  
具体可以通过下面两条命令来修改，改完之后，点击飞行模式(重启下网络), 然后关闭飞行模式连接 WiFi 就会恢复正常

```
adb shell settings put global captive_portal_https_url https://connect.rom.miui.com/generate_204
adb shell settings put global captive_portal_http_url http://connect.rom.miui.com/generate_204
```

方法二 (修改源码，永久生效)

1.  安卓9以下版本ROM修改204服务器教程：  
    源码位置：frameworks/base/services/core/java/com/android/server/connectivity/NetworkMonitor.java

```java
原代码：
private static final String DEFAULT_HTTPS_URL=”https://www.google.com/generate_204″;
private static final String DEFAULT_HTTP_URL=”http://connectivitycheck.gstatic.com/generate_204″;
private static final String DEFAULT_FALLBACK_URL=”http://www.google.com/gen_204″;
private static final String DEFAULT_OTHER_FALLBACK_URLS=”http://play.googleapis.com/generate_204″;

修改为：
private static final String DEFAULT_HTTPS_URL=”https://connect.rom.miui.com/generate_204″;
private static final String DEFAULT_HTTP_URL=”http://connect.rom.miui.com/generate_204″;
private static final String DEFAULT_FALLBACK_URL=”http://connect.rom.miui.com/generate_204″;
private static final String DEFAULT_OTHER_FALLBACK_URLS=”http://connect.rom.miui.com/generate_204″;
```

2.  安卓10以上版本ROM修改204服务器教程：  
    源码位置：packages/modules/NetworkStack/res/config.xml

```xml
原代码：
<!-- HTTP URL for network validation, to use for detecting captive portals. -->
<string name="default_captive_portal_http_url" translatable="false">http://connectivitycheck.gstatic.com/generate_204</string>
<!-- HTTPS URL for network validation, to use for confirming internet connectivity. -->
<string name="default_captive_portal_https_url" translatable="false">https://www.google.com/generate_204</string>
<!-- List of fallback URLs to use for detecting captive portals. -->
<string-array name="default_captive_portal_fallback_urls" translatable="false">
    <item>http://www.google.com/gen_204</item>
    <item>http://play.googleapis.com/generate_204</item>
</string-array>

修改为：

<!-- HTTP URL for network validation, to use for detecting captive portals. -->
<string name="default_captive_portal_http_url" translatable="false">http://connect.rom.miui.com/generate_204</string>
<!-- HTTPS URL for network validation, to use for confirming internet connectivity. -->
<string name="default_captive_portal_https_url" translatable="false">https://connect.rom.miui.com/generate_204</string>
<!-- List of fallback URLs to use for detecting captive portals. -->
<string-array name="default_captive_portal_fallback_urls" translatable="false">
    <item>http://connect.rom.miui.com/generate_204</item>
    <item>http://connect.rom.miui.com/generate_204</item>
</string-array>
```

参考：https://www.jipinsoft.com/17906.html