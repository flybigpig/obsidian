## Hook

##### Hook是什么？

Hook 又叫“钩子”，它可以在事件传送的过程中截获并监控事件的传输，将自身的代码与系统方法进行融入。

这样当这些方法被调用时，也就可以执行我们自己的代码，这也是面向切面编程的思想（AOP）。



Hook 可以是 个方也或者 个对象，它像 个钩子 样挂在对象 和对之间

当对象 调用对象 之前做 些处理（比如修改方陆的参数和返回值），起到了“欺上瞒下”的作用，与其说 Hook 是钩子，不如说是劫持来的更贴切些。

我们知道应用程序进程之间是彼此独立的，应用程序进程和系统进程之间也是如 ，想要在应用程序进程更改系统进程的某些行为很难直接实现，有了 Hook 技术，我们就可以在进程间进行行为更改



可以看到 Hook 可以将自己融入到它所要劫持的对象（对象 ）所在的进程中，成为系统进程的 部分，这样我们就可以通过 Hook 来更改对象 的行为。

被劫持的对象（对象），称作 Hook 点，为了保证 Hook 的稳定性， Hook 般选择 易找到并且不易变化的对象，**静态变量** 和**单例**就符合这 条件。





```
/***** OnClickListener
     * hook Button 组件的 getListenerInfo 方法
     * @param view
     */
    private void hookOnClick(View view){

        // 获取 View 的 getListenerInfo 方法
        Method getListenerInfo = null;
        try {
            getListenerInfo = View.class.getDeclaredMethod("getListenerInfo");
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
        }

        // 执行所有的反射方法 , 设置成员变量 之前 , 都要设置可见性
        getListenerInfo.setAccessible(true);

        // 执行 View view 对象的 getListenerInfo 方法
        Object mListenerInfo = null;
        try {
            mListenerInfo = getListenerInfo.invoke(view);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }

        // 反射获取 OnClickListener 成员
        // ① 先根据全类名获取 ListenerInfo 字节码
        Class<?> clazz = null;
        try {
            clazz = Class.forName("android.view.View$ListenerInfo");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }

        // ② 获取 android.view.View.ListenerInfo 中的 mOnClickListener 成员
        Field field = null;
        try {
            field = clazz.getField("mOnClickListener");
        } catch (NoSuchFieldException e) {
            e.printStackTrace();
        }

        // ③ 设置该字段访问性, 执行所有的反射方法 , 设置成员变量 之前 , 都要设置可见性
        field.setAccessible(true);

        // ④ 获取 mOnClickListener 成员变量
        View.OnClickListener mOnClickListener = null;
        try {
            mOnClickListener = (View.OnClickListener) field.get(mListenerInfo);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        }

        // ⑤ 修改 View 的 ListenerInfo 成员的 mOnClickListener 成员
        // 其中 ListenerInfo 成员 是
        try {
            View.OnClickListener finalMOnClickListener = mOnClickListener;
            field.set(mListenerInfo, new View.OnClickListener(){
                @Override
                public void onClick(View v) {
                    Log.i(TAG, "Hook Before");
                    finalMOnClickListener.onClick(view);
                    Log.i(TAG, "Hook After");
                }
            });
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        }
    }
```





```
/**
     * Hook Activity 界面启动过程
     * 钩住 Instrumentation 的 execStartActivity 方法
     * 向该方法中注入自定义的业务逻辑
     */
    private void hookStartActivity(){

        // 1. 获取 Activity 字节码文件
        //    字节码文件是所有反射操作的入口
        Class<?> clazz = Activity.class;

        // 2. 获取 Activity 的 Instrumentation mInstrumentation 成员 Field 字段
        Field mInstrumentation_Field = null;
        try {
            mInstrumentation_Field = clazz.getDeclaredField("mInstrumentation");
        } catch (NoSuchFieldException e) {
            e.printStackTrace();
        }

        // 3. 设置 Field mInstrumentation 字段的可访问性
        mInstrumentation_Field.setAccessible(true);

        // 4. 获取 Activity 的 Instrumentation mInstrumentation 成员对象值
        //      获取该成员的意义是 , 创建 Instrumentation 代理时, 需要将原始的 Instrumentation 传入代理对象中
        Instrumentation mInstrumentation = null;
        try {
            mInstrumentation = (Instrumentation) mInstrumentation_Field.get(this);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        }

        // 5. 将 Activity 的 Instrumentation mInstrumentation 成员变量
        //      设置为自己定义的 Instrumentation 代理对象
        try {
            mInstrumentation_Field.set(this, new InstrumentationProxy(mInstrumentation));
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        }


    }
```





```
public class InstrumentationProxy extends Instrumentation {

    private static final String TAG = "InstrumentationProxy";
    /**
     * Activity 中原本的 Instrumentation mInstrumentation 成员
     * 从构造函数中进行初始化
     */
    final Instrumentation orginalInstrumentation;

    public InstrumentationProxy(Instrumentation orginalInstrumentation) {
        this.orginalInstrumentation = orginalInstrumentation;
    }

    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,
            Intent intent, int requestCode, Bundle options) {

        Log.i(TAG, "注入的 Hook 前执行的业务逻辑");

        // 1. 反射执行 Instrumentation orginalInstrumentation 成员的 execStartActivity 方法
        Method execStartActivity_Method = null;
        try {
            execStartActivity_Method = Instrumentation.class.getDeclaredMethod(
                    "execStartActivity",
                    Context.class,
                    IBinder.class,
                    IBinder.class,
                    Activity.class,
                    Intent.class,
                    int.class,
                    Bundle.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
        }

        // 2. 设置方法可访问性
        execStartActivity_Method.setAccessible(true);

        // 3. 执行 Instrumentation orginalInstrumentation 的 execStartActivity 方法
        //    使用 Object 类型对象接收反射方法执行结果
        ActivityResult activityResult = null;
        try {
            activityResult = (ActivityResult) execStartActivity_Method.invoke(orginalInstrumentation,
                    who,
                    contextThread,
                    token,
                    target,
                    intent,
                    requestCode,
                    options);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }

        Log.i(TAG, "注入的 Hook 后执行的业务逻辑");

        return activityResult;
    }
}
```

