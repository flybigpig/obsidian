前几篇文章陆陆续续实现了HAL，HIDL，JNI，AIDL服务，现在只差最后一步，应用层的实现我们就可以打通应用层到HAL的整个调用流程了，话不多说，上代码

应用层的实现相对比较简单，在Android Studio中进行开发，写完之后拷贝到源码packages/apps下进行编译就行了，在packages/apps下创建HelloDemo目录，将开发好的文件拷贝过来，看看目录结构：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b8dcdc3f49785c9db2eda1b1557a5e95.png)

很简单的一个应用，把主要文件源码贴出来一下：

### MainActivity.java

```
package com.android.hello;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.os.IBinder;
import android.app.IHelloService;
import android.os.ServiceManager;
import android.os.RemoteException;
import android.view.Window;
public class MainActivity extends Activity implements View.OnClickListener {
    private Button button;

    private EditText editText1;
    private EditText editText2;
    private EditText editText3;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.activity_main);
        button = findViewById(R.id.button);
        editText1 = findViewById(R.id.edit_text1);
        editText2 = findViewById(R.id.edit_text2);
        editText3 = findViewById(R.id.edit_text3);
        button.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        IHelloService helloService = getHelloService();
        if(helloService == null){
           Log.d("dongjiao","onClick...faile to get helloService...");
           return;
        }
        Log.d("dongjiao","onClick...success to get helloService...");
        int a = Integer.parseInt(editText1.getText().toString());
        int b = Integer.parseInt(editText2.getText().toString());
        try{
           int total = helloService.add(a,b);
           editText3.setText(total+"");
           Toast.makeText(this,"total = :"+total,Toast.LENGTH_LONG).show();
        }catch(RemoteException e){
             Log.d("dongjiao","RemoteException = :"+e);
        }
    }
    private IHelloService getHelloService(){
         IBinder b = ServiceManager.getService("helloService");
         IHelloService helloService = IHelloService.Stub.asInterface(b);
         return helloService;
   }
}

```

### activity\_main.xml

```
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:app="http://schemas.android.com/apk/res-auto"
    xmlns:tools="http://schemas.android.com/tools"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:orientation="vertical"
    tools:context=".MainActivity">
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="horizontal">
        <EditText
            android:id="@+id/edit_text1"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:hint="请输入"
            android:inputType="number"
            />
        <EditText
            android:id="@+id/edit_text2"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:hint="请输入"
            android:inputType="number"
            />
        <EditText
            android:id="@+id/edit_text3"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:hint="0"
            />
    </LinearLayout>

    <Button
        android:id="@+id/button"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:text="click"
        />
</LinearLayout>
```

### AndroidManifest.xml

```
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.android.hello">

    <application
        android:allowBackup="true"
        android:icon="@drawable/ic_launcher"
        android:label="HelloDemo"
        android:roundIcon="@drawable/ic_launcher"
        android:supportsRtl="true"
        android:theme="@android:style/Theme.Material.Light">
        <activity android:name=".MainActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />

                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>

</manifest>
```

### Android.bp

```
android_app {
    name: "HelloDemo",
    srcs: ["src/**/*.java"],
    platform_apis: true,
}
```

这个应用功能就是点击Button时，获取前一篇文章实现的HelloService服务，然后调用HelloService的add方法，这个方法会通过JNI到HIDL服务中

接着进行编译，mmm packages/apps/HelloDemo

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c2397b30005dd778de681df0aa51017f.png)  
编译成功后将这个APK push到手机中  
adb push out/target/product/TOKYO\_TF\_arm64/system/app/HelloDemo/HelloDemo.apk /system/app/HelloDemo  
重启手机，这个APK界面就这样：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/409c892aa7deed8629aab413e06a381e.png)  
功能很简单，三个EditText和一个Button，前两个EditText用来接收两个加数，后一个EditText接收它们的和，通过Button调用HelloService的add方法，HelloService的add方法通过JNI到native层再到HIDL调用addition\_hidl函数，最终到HAL中调用additionTest函数实现两个数相加，然后返回给APP，  
我们最终目的就是如此

我在HAL，HIDL，JNI，AIDL，APP各层都添加了log，来测试一下：

1.  关闭SELinux权限，adb shell setenforce 0
2.  启动HIDL service  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9dbe9311b436003d6e0a018fb1d7dc01.png)
3.  杀掉system\_server让AIDL服务HelloService重新启动，HelloService启动的同时会通过JNI获取HIDL服务，HIDL服务中又会获取HAL

如下是AIDL服务HelloService启动的log输出  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/61354263991ef12e530b11530b599de7.png)  
log中HAL，HIDL，JNI，AIDL每层都已经成功输出log，说明我们在SystemService中启动HelloService之后到HAL的流程已经打通了（HAL，HIDL，JNI，AIDL可参考前几篇文章实现），再测试一下APP的功能，输入两个数，点击CLICK按钮  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/127b46c4ae8892aed762ccca728f79bf.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/85551067a6ce1eea3c7929e78c604811.png)  
没有报错，成功计算出total值，看看log输出：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9ae5b3839289dd52bcd6abed2cab8272.png)  
log也很清晰，从APP的click开始，到AIDL，JNI，HIDL，HAL  
功能虽简单，麻雀虽小，五脏俱全，到这里我们这个系列的文章已经结束，从应用层到HAL的整体流程已经了解，其实Android系统的整体流程就是这样的，只不过它每一层包含复杂的逻辑，但是架构都类似，上层到底层，层层调用，一个APP就可以简单操纵硬件，一层一层提供接口

了解系统调用流程对我们理解Android系统的帮助是非常大的

这几篇文章的源码已经上传github，执行如下命令即可获取：  
git clone https://github.com/tdj520/-HAL.git