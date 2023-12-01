# Opengl ES

##### GLSurfaceView

> glsurfaceview 实际上为自己创建了一个窗口。并在视图层次View Hierarchy上穿个洞。让底层的OpenGL surface 显示出来。但是 GLsurfaceview 与常规视图不同。 它没有动画或者变形特效，因为GLSurfaceView 是窗口的一部分 。

> glsurfaceview会显示设备刷新频率不断地渲染，当然，只需要用GlSurfaceView.RENDERMODE_WHEN_DIRTY 作为参数调用 Glsurfaceview.setRenderMode()即可。
>
> 主线程中的GLsurfaceView 实例可以调用queueEvent() 传一个Runnable给后台渲染线程。渲染线程可以调用Activity的runOnUiThread()来传递时间给主线程。

> 从4.0开始，Android提供了一个纹理视图--TexttrueView ,它可以渲染OpenGl 而不是创建窗口或者打洞。这个视图像一个常规窗口一样，可以被操作，且有动画和变形特效，但是 TextrueView类没有内置的OpenGl初始化操作。要想使用TextureView,一种方法是执行自定义的OpenGl初始化，并且再TextureView上运行。另外一种方法是把GLsurfaceview源代码拿出来。适配到TextureView。



**在OpenGl 里 只能绘制点、直线、以及三角形**

> 定义三角形时，总是逆时针的顺序排列顶点，这称为 卷曲顺序，可以优化性能。