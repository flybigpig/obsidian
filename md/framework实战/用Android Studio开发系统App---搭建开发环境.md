
直接使用Android Studio可以开发普通的App，如果要开发系统App并使之能够在目标机上调试，则需要解决以下几个问题：

1.  调用被Google隐藏起来的API
2.  使用系统级别的API和权限
3.  使开发的App能运行在目标机上

本篇文章记录如何配置Android Studio开发环境来解决问题1。  
问题2、3的解决请参考文章：[https://blog.csdn.net/cxq234843654/article/details/51557025](https://blog.csdn.net/cxq234843654/article/details/51557025)

开发环境版本：compileSdkVersion 27， Android Studio 3.3

### 一、获取将@hide属性标签祛除的Framework Jar包

这一步要在Linux环境下完成：

1.  祛除Android源码中关联API的@hide标签
2.  编译Android源码
3.  获取生成的Framework Jar包（classes-full-debug.jar），更名为framework.jar

### 二、配置Android Studio工程属性

1.  将上一步获取的framework.jar拷贝到工程目录下的\\app\\libs文件夹中。
2.  Project目录结构下，鼠标右击后选择add as library，将包添加到环境变量中。
3.  Android目录结构下，打开Project的build.gradle文件，在allprojects模块中添加如下内容：

```javascript
allprojects {
   repositories {
       // 省略无关内容
   }
   // 需要添加的内容
   gradle.projectsEvaluated {
       tasks.withType(JavaCompile){
           options.compilerArgs.add('-Xbootclasspath/p:app/libs/framework.jar')
       }
   }
}
```

4.  Android目录结构下，在app的build.gradle中依赖库的第一行添加如下内容，防止两套Framework导致数组越界的问题。

```javascript
dependencies {
   // 需要添加的内容
   compileOnly files('libs/framework.jar')
   // 省略无关内容
}
```

5.  Project目录结构下，在app/app.iml中修改Framework.jar的调用优先级，将如下系统SDK的order定义放到最后面：

```javascript
<orderEntry type="jdk" jdkName="Android API 27 Platform" jdkType="Android SDK" />
```

以上5步完成后就可以使用Android Studio开发系统App了。  
第5步有时候不起作用，因为同步gradle时，它会自动跑到第一条。但是不影响编译，只是会有报错的红线，以及这些API无法自动补齐。

参考文章：  
[http://www.cnblogs.com/startkey/p/10042194.html](http://www.cnblogs.com/startkey/p/10042194.html)