## Android FlexboxLayout流式布局

FlexBoxLayout是为Android带来了与 CSS Flexible Box Layout（CSS 弹性盒子）相似功能的库。  
**一：添加依赖**  
如果迁移的AndroidX中

```
dependencies {
    implementation 'com.google.android:flexbox:2.0.1'
}
```

否则：

```
dependencies {
    implementation 'com.google.android:flexbox:1.0.0'
}
```

**二：实例：**  
布局添加：

```
  <com.google.android.flexbox.FlexboxLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        app:flexWrap="wrap">
        <TextView
            style="@style/TextStyle"
            android:text="你真的这样的世界吗"/>

        <TextView
            style="@style/TextStyle"
            android:text="你真的了解中国吗" />

        <TextView
            style="@style/TextStyle"
            android:text="你真的了解你自己吗"/>

        <TextView
            style="@style/TextStyle"
            android:text="你了解你的父母吗" />

        <TextView
            style="@style/TextStyle"
            android:text="你..." />

        <TextView
            style="@style/TextStyle"
            android:text="我...." />

        <TextView
            style="@style/TextStyle"
            android:text="他...." />

    </com.google.android.flexbox.FlexboxLayout>
```

TextStyle 布局风格

```
 <style name="TextStyle">
        <item name="android:layout_margin">5dp</item>
        <item name="android:layout_width">wrap_content</item>
        <item name="android:layout_height">wrap_content</item>
        <item name="android:background">@drawable/shape_border</item>
        <item name="android:ellipsize">end</item>
        <item name="android:maxLines">1</item>
        <item name="android:padding">8dp</item>
        <item name="android:textColor">@android:color/black</item>
```

在drawable下创建shape\_border.xml

```
<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android">
    <corners android:radius="12dp"/>
    <stroke android:color="@color/red" android:width="2dp"/>

</shape>
```

**三、FlexboxLayout属性介绍**

1.  app:flexWrap="nowrap"//该属性表示是否换行和换行的方向  
    flexWrap属性一共有三个枚举值，分别是nowrap、wrap和wrap\_reverse  
    nowrap:单行显示，不换行，默认就是这个属性。  
    wrap：当内容超过一行时，自动换行。  
    wrap\_reverse ：反向换行（下一行内容在当前行之上）  
    ![image.png](https://segmentfault.com/img/bVcRIZo "image.png")
2.  app:flexDirection="column"//主轴方向按竖直（列）方向排版。效果：  
    ![image.png](https://segmentfault.com/img/bVcRIZN "image.png")  
    column\_reverse : 主轴方向按竖直（列）方向反向排版（从下向上）。  
    row : 主轴方向按水平（行）方向排版，默认就是这个属性  
    row\_reverse : 主轴方向按水平（行）方向反向排版
3.  app:justifyContent="flex\_start"  
    关于这个属性，官方有一段说明：

```
<!-- Omitting flex-flow property since it's reflected in the parent flex container. Set the flexDirection and/or flexWrap to the parent flex container explicitly if you want to use the flex-flow similar way to the web. -->
```

作用是控制元素在主轴上的对齐方式，需要配合flexDirection或flexWrap属性来使用。  
看一下源码，可见app:justifyContent属性有flex\_start、flex\_end、center、space\_between、space\_around和space\_evenly6个枚举值。  
<com.google.android.flexbox.FlexboxLayout

```
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    app:flexWrap="wrap"
    app:flexDirection="row"
    app:justifyContent="flex_end">
    
```

**_app:flexDirection 得是row 是column 没效果_**  
flex\_start: 左对齐，默认值。  
flex\_end : 右对齐。  
![image.png](https://segmentfault.com/img/bVcRI0E "image.png")

center：居中对齐。  
space\_between: 两端对齐。  
space\_around : 分散对齐。  
space\_evenly:子元素在一行内均匀分布空间。

4.app:alignItems="stretch"//该属性表示元素在每条轴线上的对齐方式。  
注意：如果设置了alignContent，且值不为stretch,那么该属性失效。  
我们改变每一子View的布局Padding 看看效果

```
  <TextView
            style="@style/TextStyle"
            android:text="你真的这样的世界吗"
            android:padding="10dp"/>

        <TextView
            style="@style/TextStyle"
            android:text="你真的了解中国吗"
            android:padding="20dp"/>

        <TextView
            style="@style/TextStyle"
            android:text="你真的了解你自己吗"
            android:padding="5dp"/>

        <TextView
            style="@style/TextStyle"
            android:text="你了解你的父母吗"
            android:padding="15dp"/>
```

app:alignItems属性有flex\_start、flex\_end、center、baseline和stretch  
stretch: 默认值，如果子元素未设置高度，则沾满父布局高度。  
flex\_start :顶端对齐  
![image.png](https://segmentfault.com/img/bVcRI2N "image.png")  
flex\_end :底端对齐。  
center : 居中对齐。  
baseline :按照第一个元素的基线对齐。  
下面这张图片可以很直观的表达这个属性的作用  
![image.png](https://segmentfault.com/img/bVcRI2Q "image.png")

1.  app:alignContent="stretch"//该属性表示每条轴线在整个布局中的对齐方式。  
    app:alignContent属性有flex\_start、flex\_end、center、space\_between、space\_around和stretch6个枚举值。

```
 stretch:默认值，轴线占满整个父布局。
 flex_start:顶部对齐所有轴线
 flex_end:底部对齐所有轴线。
 center:居中对齐所有轴线。
 space_between:两端对齐所有轴线。
 space_around:分散对齐所有轴线。
```

6.dividerDrawable （reference）//水平和竖直方向分割线。

1.  dividerDrawableHorizontal / dividerDrawableVertical （reference）//水平/竖直方向分割线。
2.  app:showDivider="middle"//显示水平和竖直方向分割线方式。
    
    ```
    <com.google.android.flexbox.FlexboxLayout
         android:layout_width="match_parent"
         android:layout_height="wrap_content"
         app:flexWrap="wrap"
         app:flexDirection="row"
         app:alignItems="flex_start"
         app:dividerDrawable="@color/red"
         app:showDivider="middle">
    ```
    
    ![image.png](https://segmentfault.com/img/bVcRI3r "image.png")  
    9.app:layout\_order="2"//指定子元素排序优先级，值越小越排在前面，默认值为1。设置值类型为float。
    

```
 <TextView
            style="@style/TextStyle"
            android:text="你真的这样的世界吗"
            android:padding="10dp"
            app:layout_order="2"
            />
```

这样的话这样一条就排到最后去了

1.  app:layout\_flexShrink="0"  
    子元素缩放比例，如果设置了换行（flexWrap=“wrap或wrap\_reverse”）则该属性无效。  
    设置值类型为float，0表示不缩放，数值越大，缩放比例越大，默认为1，负值无效。
2.  app:layout\_flexGrow="0"  
    分配同一轴线剩余控件所占权重，默认值为0，表示不参与分配。用法类似于LinearLayout的weight，不过weight分配的是整个父布局控件，而layout\_flexGrow分配的是同一行/列的剩余空间。  
    **四：与RecyclerView配合使用**

```
public class FourActivity extends AppCompatActivity {
    private RecyclerView recycler;
    private FlexAdapter flexAdapter;
    private List<String> mDatas=new ArrayList<>();
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_four_one);
        initData();
        recycler= findViewById(R.id.recycler);
        FlexboxLayoutManager manager=new FlexboxLayoutManager(this);
        //设置主轴排列方式
        manager.setFlexDirection(FlexDirection.ROW);
        //设置是否换行
        manager.setFlexWrap(FlexWrap.WRAP);
        
         //manager.setAlignItems(AlignItems.FLEX_END);
        recycler.setLayoutManager(manager);
        flexAdapter=new FlexAdapter(this,mDatas);
        recycler.setAdapter(flexAdapter);
    }

    private void initData() {
        mDatas.add("我真的ok");
        mDatas.add("你好世界");
        mDatas.add("世界");
        mDatas.add("你是一个AAA什么");
        mDatas.add("我真的AAAok");
        mDatas.add("你好世界AAA");
        mDatas.add("世界AAA");
        mDatas.add("你是一个什么AAA");
    }
}
```

FlexboxLayoutManager 可以替换其他布局管理器实现流式布局  
由于RecyclerView 的一些特性，Flexbox 的一些属性在FlexboxLayoutManager中没有实现，下面是FlexboxLayout和FlexboxLayoutManager支持的属性的对比：  
![image.png](https://segmentfault.com/img/bVcRJh0 "image.png")

**END:螃蟹在剥我的壳,笔记本在写我。漫天的我落在枫叶上雪花上。而你在想我。**