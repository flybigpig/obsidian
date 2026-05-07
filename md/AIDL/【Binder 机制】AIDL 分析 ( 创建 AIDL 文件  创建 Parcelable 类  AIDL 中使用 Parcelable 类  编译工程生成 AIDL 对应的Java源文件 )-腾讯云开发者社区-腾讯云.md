#### 文章目录

-   [一、创建 AIDL 文件](https://cloud.tencent.com/developer?from_column=20421&from=20421)
-   -   [1、创建 AIDL 目录](https://cloud.tencent.com/developer?from_column=20421&from=20421)
    -   [2、创建 AIDL 文件](https://cloud.tencent.com/developer?from_column=20421&from=20421)
    -   [3、创建 Parcelable 类](https://cloud.tencent.com/developer?from_column=20421&from=20421)
    -   [4、AIDL 目录下声明 Parcelable 类](https://cloud.tencent.com/developer?from_column=20421&from=20421)
    -   [5、AIDL 中使用 Parcelable 类](https://cloud.tencent.com/developer?from_column=20421&from=20421)
-   [二、编译工程生成 AIDL 文件对应的 Java 源文件](https://cloud.tencent.com/developer?from_column=20421&from=20421)
-   -   [1、编译工程](https://cloud.tencent.com/developer?from_column=20421&from=20421)
    -   [2、生成的 AIDL 对应 Java 源文件](https://cloud.tencent.com/developer?from_column=20421&from=20421)

___

### 1、创建 AIDL 目录

在 Android Studio 工程中 , 创建 aidl 文件 ;

右键点击 main 目录 , 选择 " New / Directory " ,

![](https://developer.qcloudimg.com/http-save/yehe-2542479/138160ef4460fdd6ffb95bded4bc1f47.png)

选择创建 " aidl " 目录 ,

![](https://developer.qcloudimg.com/http-save/yehe-2542479/f6fb8dbb36b5738e5d4cb378da4af80b.png)

**创建好的 aidl 目录如下 :**

![](https://developer.qcloudimg.com/http-save/yehe-2542479/130325f1e77b1e7125772874fdaf714d.png)

### 2、创建 AIDL 文件

右键点击 " aidl " 目录 , 选择 " New / AIDL / AIDL File " 文件 ;

![](https://developer.qcloudimg.com/http-save/yehe-2542479/e6218bab6ad8a75317faf0713cfbb8ef.png)

弹出对话框 , 输入 AIDL 接口名称 , 输入完毕后 , 选择 " Finish " 选项 ;

![](https://developer.qcloudimg.com/http-save/yehe-2542479/c8187ed268ccf46a29bb77302eb81a21.png)

### 3、创建 Parcelable 类

创建 Parcelable 类 :

```javascript
package kim.hsl.aidl_demo; import android.os.Parcel; import android.os.Parcelable; public class Student implements Parcelable { private String name; public Student(String name) { this.name = name; } public String getName() { return name; } public void setName(String name) { this.name = name; } protected Student(Parcel in) { name = in.readString(); } public static final Creator<Student> CREATOR = new Creator<Student>() { @Override public Student createFromParcel(Parcel in) { return new Student(in); } @Override public Student[] newArray(int size) { return new Student[size]; } }; @Override public int describeContents() { return 0; } @Override public void writeToParcel(Parcel dest, int flags) { dest.writeString(name); } public void readFromParcel(Parcel desc) { name = desc.readString(); } @Override public String toString() { return "name=" + name; } }
```

### 4、AIDL 目录下声明 Parcelable 类

**在 aidl 目录下声明 Parcelable 类 :** 在 aidl 目录下创建 Student.aidl 文件 , 然后声明如下内容 ;

```javascript
package kim.hsl.aidl_demo; parcelable Student;
```

![](https://developer.qcloudimg.com/http-save/yehe-2542479/ad8ae7957b7625534942e4ad4ee842a8.png)

### 5、AIDL 中使用 Parcelable 类

**在创建的 AIDL 接口中使用 Student 类 :**

① 首先要导入 Student 类 , `import kim.hsl.aidl_demo.Student;`

② 参数的输入输出 , in 写入, out 输出, inout 写入和输出 ;

```javascript
// IMyAidlInterface.aidl package kim.hsl.aidl_demo; import kim.hsl.aidl_demo.Student; // Declare any non-default types here with import statements interface IMyAidlInterface { /** * Demonstrates some basic types that you can use as parameters * and return values in AIDL. */ void basicTypes(int anInt, long aLong, boolean aBoolean, float aFloat, double aDouble, String aString); /** * in 写入, out 输出, inout 写入和输出 */ void addStudent(inout Student student); /** * 获取 Student 集合 */ List<Student> getStudents(); }
```

![](https://developer.qcloudimg.com/http-save/yehe-2542479/f07edea91f39dd9f4cb47a2d47373d36.png)

## 二、编译工程生成 AIDL 文件对应的 Java 源文件

___

### 1、编译工程

点击 " 菜单栏 / Build / Make Project " 选项 , 即可编译当前的工程 , 进而生成 AIDL 接口对应的 Java 源文件 ;

![](https://developer.qcloudimg.com/http-save/yehe-2542479/e12e628595b43f14ed82d82b82454576.png)

编译后 , 在 " AIDL\_Demo\\app\\build\\generated\\aidl\_source\_output\_dir\\debug\\out\\kim\\hsl\\aidl\_demo " 目录 , 生成了 AIDL 文件对应的源码 :

![](https://developer.qcloudimg.com/http-save/yehe-2542479/fb110138f66d750274b83e1560c684fa.png)

### 2、生成的 AIDL 对应 Java 源文件

**下面的源码是编译生成的 Java 源文件 :**

```javascript
/* * This file is auto-generated. DO NOT MODIFY. */ package kim.hsl.aidl_demo; // Declare any non-default types here with import statements public interface IMyAidlInterface extends android.os.IInterface { /** Default implementation for IMyAidlInterface. */ public static class Default implements kim.hsl.aidl_demo.IMyAidlInterface { /** * Demonstrates some basic types that you can use as parameters * and return values in AIDL. */ @Override public void basicTypes(int anInt, long aLong, boolean aBoolean, float aFloat, double aDouble, java.lang.String aString) throws android.os.RemoteException { } /** * in 写入, out 输出, inout 写入和输出 */ @Override public void addStudent(kim.hsl.aidl_demo.Student student) throws android.os.RemoteException { } /** * 获取 Student 集合 */ @Override public java.util.List<kim.hsl.aidl_demo.Student> getStudents() throws android.os.RemoteException { return null; } @Override public android.os.IBinder asBinder() { return null; } } /** Local-side IPC implementation stub class. */ public static abstract class Stub extends android.os.Binder implements kim.hsl.aidl_demo.IMyAidlInterface { private static final java.lang.String DESCRIPTOR = "kim.hsl.aidl_demo.IMyAidlInterface"; /** Construct the stub at attach it to the interface. */ public Stub() { this.attachInterface(this, DESCRIPTOR); } /** * Cast an IBinder object into an kim.hsl.aidl_demo.IMyAidlInterface interface, * generating a proxy if needed. */ public static kim.hsl.aidl_demo.IMyAidlInterface asInterface(android.os.IBinder obj) { if ((obj==null)) { return null; } android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR); if (((iin!=null)&&(iin instanceof kim.hsl.aidl_demo.IMyAidlInterface))) { return ((kim.hsl.aidl_demo.IMyAidlInterface)iin); } return new kim.hsl.aidl_demo.IMyAidlInterface.Stub.Proxy(obj); } @Override public android.os.IBinder asBinder() { return this; } @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException { java.lang.String descriptor = DESCRIPTOR; switch (code) { case INTERFACE_TRANSACTION: { reply.writeString(descriptor); return true; } case TRANSACTION_basicTypes: { data.enforceInterface(descriptor); int _arg0; _arg0 = data.readInt(); long _arg1; _arg1 = data.readLong(); boolean _arg2; _arg2 = (0!=data.readInt()); float _arg3; _arg3 = data.readFloat(); double _arg4; _arg4 = data.readDouble(); java.lang.String _arg5; _arg5 = data.readString(); this.basicTypes(_arg0, _arg1, _arg2, _arg3, _arg4, _arg5); reply.writeNoException(); return true; } case TRANSACTION_addStudent: { data.enforceInterface(descriptor); kim.hsl.aidl_demo.Student _arg0; if ((0!=data.readInt())) { _arg0 = kim.hsl.aidl_demo.Student.CREATOR.createFromParcel(data); } else { _arg0 = null; } this.addStudent(_arg0); reply.writeNoException(); if ((_arg0!=null)) { reply.writeInt(1); _arg0.writeToParcel(reply, android.os.Parcelable.PARCELABLE_WRITE_RETURN_VALUE); } else { reply.writeInt(0); } return true; } case TRANSACTION_getStudents: { data.enforceInterface(descriptor); java.util.List<kim.hsl.aidl_demo.Student> _result = this.getStudents(); reply.writeNoException(); reply.writeTypedList(_result); return true; } default: { return super.onTransact(code, data, reply, flags); } } } private static class Proxy implements kim.hsl.aidl_demo.IMyAidlInterface { private android.os.IBinder mRemote; Proxy(android.os.IBinder remote) { mRemote = remote; } @Override public android.os.IBinder asBinder() { return mRemote; } public java.lang.String getInterfaceDescriptor() { return DESCRIPTOR; } /** * Demonstrates some basic types that you can use as parameters * and return values in AIDL. */ @Override public void basicTypes(int anInt, long aLong, boolean aBoolean, float aFloat, double aDouble, java.lang.String aString) throws android.os.RemoteException { android.os.Parcel _data = android.os.Parcel.obtain(); android.os.Parcel _reply = android.os.Parcel.obtain(); try { _data.writeInterfaceToken(DESCRIPTOR); _data.writeInt(anInt); _data.writeLong(aLong); _data.writeInt(((aBoolean)?(1):(0))); _data.writeFloat(aFloat); _data.writeDouble(aDouble); _data.writeString(aString); boolean _status = mRemote.transact(Stub.TRANSACTION_basicTypes, _data, _reply, 0); if (!_status && getDefaultImpl() != null) { getDefaultImpl().basicTypes(anInt, aLong, aBoolean, aFloat, aDouble, aString); return; } _reply.readException(); } finally { _reply.recycle(); _data.recycle(); } } /** * in 写入, out 输出, inout 写入和输出 */ @Override public void addStudent(kim.hsl.aidl_demo.Student student) throws android.os.RemoteException { android.os.Parcel _data = android.os.Parcel.obtain(); android.os.Parcel _reply = android.os.Parcel.obtain(); try { _data.writeInterfaceToken(DESCRIPTOR); if ((student!=null)) { _data.writeInt(1); student.writeToParcel(_data, 0); } else { _data.writeInt(0); } boolean _status = mRemote.transact(Stub.TRANSACTION_addStudent, _data, _reply, 0); if (!_status && getDefaultImpl() != null) { getDefaultImpl().addStudent(student); return; } _reply.readException(); if ((0!=_reply.readInt())) { student.readFromParcel(_reply); } } finally { _reply.recycle(); _data.recycle(); } } /** * 获取 Student 集合 */ @Override public java.util.List<kim.hsl.aidl_demo.Student> getStudents() throws android.os.RemoteException { android.os.Parcel _data = android.os.Parcel.obtain(); android.os.Parcel _reply = android.os.Parcel.obtain(); java.util.List<kim.hsl.aidl_demo.Student> _result; try { _data.writeInterfaceToken(DESCRIPTOR); boolean _status = mRemote.transact(Stub.TRANSACTION_getStudents, _data, _reply, 0); if (!_status && getDefaultImpl() != null) { return getDefaultImpl().getStudents(); } _reply.readException(); _result = _reply.createTypedArrayList(kim.hsl.aidl_demo.Student.CREATOR); } finally { _reply.recycle(); _data.recycle(); } return _result; } public static kim.hsl.aidl_demo.IMyAidlInterface sDefaultImpl; } static final int TRANSACTION_basicTypes = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0); static final int TRANSACTION_addStudent = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1); static final int TRANSACTION_getStudents = (android.os.IBinder.FIRST_CALL_TRANSACTION + 2); public static boolean setDefaultImpl(kim.hsl.aidl_demo.IMyAidlInterface impl) { // Only one user of this interface can use this function // at a time. This is a heuristic to detect if two different // users in the same process use this function. if (Stub.Proxy.sDefaultImpl != null) { throw new IllegalStateException("setDefaultImpl() called twice"); } if (impl != null) { Stub.Proxy.sDefaultImpl = impl; return true; } return false; } public static kim.hsl.aidl_demo.IMyAidlInterface getDefaultImpl() { return Stub.Proxy.sDefaultImpl; } } /** * Demonstrates some basic types that you can use as parameters * and return values in AIDL. */ public void basicTypes(int anInt, long aLong, boolean aBoolean, float aFloat, double aDouble, java.lang.String aString) throws android.os.RemoteException; /** * in 写入, out 输出, inout 写入和输出 */ public void addStudent(kim.hsl.aidl_demo.Student student) throws android.os.RemoteException; /** * 获取 Student 集合 */ public java.util.List<kim.hsl.aidl_demo.Student> getStudents() throws android.os.RemoteException; }
```

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2021-09-16，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除