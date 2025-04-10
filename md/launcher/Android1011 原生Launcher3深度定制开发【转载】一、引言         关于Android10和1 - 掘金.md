## [](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2FCGTAIHYQ%2Farticle%2Fdetails%2F122047301%3Fspm%3D1001.2100.3001.7377%26utm_medium%3Ddistribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase%26depth_1-utm_source%3Ddistribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase "https://blog.csdn.net/CGTAIHYQ/article/details/122047301?spm=1001.2100.3001.7377&utm_medium=distribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase&depth_1-utm_source=distribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase")

## 一、引言

        关于Android10和11系统Launcher3的定制有很多，根据项目的需求会进行各种定制开发，

        于是就需要研究Launcher3的源码。本文主要从Android 11的Launcher3QuickStep着手

        （go版本或者其他版本类似）从常用的修改进行分析，首先就得大致理解 Launcher3各个

        类的作用。

###         1.1、常用类简述

                1. \*\*BaseIconFactory.java：\*\*Launcher图标的工厂类，控制图标UI展示（图标白边控制）

                    Android8,9,10可能在其他类，可以全局搜索normalizeAndWrapToAdaptiveIcon方法

                    差不多一样的逻辑。

                2. \*\*OverviewToAllAppsTouchController.java：\*\*横向控制抽屉式All应用界面的触摸类。

                3. \*\*PortraitStatesTouchController.java：\*\*竖向控制抽屉式All应用界面的触摸类。

                4. \*\*BubbleTextView.java：\*\*Launcher所有图标文字显示的父类，包括文字的大小，文字

                     刷新的的父类。

                5. \*\*CellLayout.java：\*\*Launcher布局的计算类，图标的显示边距等，组成workspace

                    的view,继承自viewgroup，既是一个dragSource又是一个dropTarget,可以将它里面

                    的item拖出去，也可以容纳拖动过来的item。在workspace\_screen里面定了一些它

                    的view参数。

                6. \*\*SecondaryDropTarget.java：\*\*长按APP图标的操作类，对图标进行移动、删除、

                     移除、取消、卸载等操作。

                7. \*\*DeviceProfile.java：\*\*图标大小、各个图标间距，布局等计算实体类，可配置各个参数

                    的全局变量。

                8. **Launcher.java:** launcher主要的activity，是launcher桌面第一次启动的activity，UI的

                    主要入口。

                9. **Workspace.java:** 抽象的桌面。由N个cellLayout组成,从cellLayout更高一级的层面上

                    对事件的处理。

                10. **ClippedFolderIconLayoutRule.java:** 文件夹图标内部显示小图标缩略图的计算类

                      ，常见4宫格9宫格显示的实现类。

                11. **FolderGridOrganizer.java:** 展开文件夹显示的计算逻辑类，文件夹图标呈现是网格

                     状，此类主要给文件夹各应用图标制定显示规则，比如3\*4,4\*4。

                12. **LoaderTask.java:** 加载各个模块Task的显示类，如workspace工作区icon、all工作

                      区icon初始化工作。

                13. **PackageUpdatedTask.java:** PMS安装应用后更新Launcher3图标及逻辑的实现类。

                14. \*\*device\_profiles.xml：\*\*默认Launcher的网格配置，主要包括一下几点：

                       a. workspace的行和列；

                       b. 文件夹中配置的行和列；

                       c. 图标大小；

                       d. 图标名称文字大小；

                       e. 默认选择加载哪个网格xml的配置文件。

                15. \*\*default\_workspace\_xxx.xml/partner\_default\_layout.xml：\*\*默认排序各个图标位

                    置的配置文件，包括文件夹默认创建显示及位置。

                16. \*\*folder\_shapes.xml：\*\*默Workspace工作区图标的圆角大小控制配置文件。

###         1.2、此文涉及修改点 

                1. 去除抽屉功能，屏蔽上拉事件与指示UI；

                2. 去除hotseat功能，包括UI的显示与拖拉事件的修改；

                3. 去除文件夹背景，以壁纸为背景；

                4. 预制及安装的应用快捷图标默认显示在Workspace区；

                5. 制定预制应用默认快捷图标在桌面显示的位置（Workspace以网格形式进行摆设），

                    出厂默认显示文件夹图标及功能；

                6. Workspace和ALL界面及文件夹展开界面图标大小，间距的定制（固定大小）；

                7. Workspace显示图标名称，字体的大小间距调整；

                8. 文件夹图标9宫格的显示；

                9. 文件夹图标名字显示，图标圆角，背景透明；

                10. 展开的文件夹多个分页名称居中，靠左显示，窗口居中，背景圆角，图标固定模板

                      网格显示（如3\*4）；

                11. 长按卸载APP的修改，防卸载功能控制，图标白边；

## 二、功能点阐述及修改

        **2.1、去除抽屉功能**

                Launcher3的抽屉功能其实就是手指滑动底部屏幕显示出一个展示所有app快捷图标入口

                的界面，根据需要，如需要去除此界面及功能，有很多方式，此次介绍最简单的方式，

                就是屏蔽手指上划事件，达到去除此功能的效果，修改如下：

                1. 竖向抽屉修改点：

                        事件派发控制在PortraitStatesTouchController类的canInterceptTouch方法

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7f5aa963af044a85b518fe7f6756760c~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                2. 横向抽屉修改点：

                        事件派发控制在OverviewToAllAppsTouchController类的canInterceptTouch方法

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/cad1222290174cb4be0e5d9392211170~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.2、去除hotseat功能

                hotseat功能就是底部一排应用快捷按钮，可放置各个应用图标及文件夹图标，支持默认

                预制应用、拖拉移除、合并成文件夹等功能，现常见的做法为移除此栏，所有图标在

                Workspace区域显示，去除此功能修改点如下：

                1. hotseat.xml布局文件中修改width为1dp与height为0dp：

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/19e8d2948b3d44b9a63fee6babf88b15~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                2. Hotseat.java实现类的构造方法及刷新时GONE布局，去除工作区移动APP到

                    hotseat区域后图标显示在hotseat区域逻辑，修改点如下：

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c61d9f6f17db4037ba5a089b408e0270~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5c8e584cdbcf45148bcf874afba2d43e~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c79058f8d3a8443689bbaaf1b268fb61~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                3. Launcher.java初始化控件的setupViews方法中GONE布局：

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1f417ddbaf6b45f48d9222ac2af150b4~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                4. LauncherPreviewRenderer.java 的MainThreadRenderer内部类中GONE布局：

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1238be06752b433388b42e06efd3791f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.3、去除文件夹背景

                文件夹缩率图背景和展开文件夹背景根据系统值或者主题设置成纯色的，不美观，根据

                需要可修改被壁纸默认的背景，比较美观大方，修改点如下：

                        FolderAnimationManager.java 类的getAnimator方法中屏蔽背景的设置，变量

                        mFolderBackground设置背景，如图：

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4c80860aa53e46b790e2570c87a2bfd7~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.4、系统预制应用显示在Workspace工作区

                Launcher3默认情况系统预制应用显示在all工作区，Workspace区只显示xml配置中显示

                的几个图标，如需要预制应用都显示Workspace工作区，需要做以下修改：

                        LoaderTask.java类中的run方法把加载的应用添加到workspace工作区

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/07a38c5975a548aab3cb30917b0dbd8a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.5、安装的应用显示在Workspace工作区

                Launcher3默认情况安装的应用显示在all工作区，如需在安装的应用在workspace工作区

                显示，需要添加新逻辑把应用添加到workspace工作区，修改如下：

                        1. PackageUpdatedTask.java类添加updateToWorkSpace方法，此方法实现安装的

                            APP添加到workspace工作区的逻辑：

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c58d92caeed14ed2a558b0c892731be3~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1b88b47aa8e44adb88e2db169c14e276~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        2. execute方法中添加updateToWorkSpace方法更新APP显示到workspace工作区

                             ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/55834bcf216840ca9ad2f9c75046f085~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.6、应用默认显示位置

                        1. device\_profiles.xml文件中配置图标显示的矩阵(包括文件夹)，图标大小及图标名

                            称大小等配置

                            ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4a34b58410c847cda60e8a313ebd7fb6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)  
2.创建一个default\_workspace\_4x5.xml(名称根据需求定义)，在device\_profiles.xml

                           文件中引用此xml

                            ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/003ec71f395543fdb05a61903d495a08~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.7、工作区、展开文件夹固定图标大小及固定图标间距

                        1. DeviceProfile类的updateIconSize方法修改workspace工作区图标的大小：

                                     iconSizePx表示图标大小，可重新赋值；

                                     iconTextSizePx表示图标大小，可重新赋值；

                                     folderIconSizePx表示文件夹图标大小，可重新赋值；

                        2. DeviceProfile类的updateFolderCellSize方法修改展开文件夹图标的小及间距：

                                     folderChildIconSizePx表示展开文件夹内图标大小，可重新赋值；   

                                     folderChildTextSizePx表示展开文件夹内图标文字大小，可重新赋值；       

                                     folderCellWidthPx表示展开文件夹内图标横向间距，可重新赋值；

                                     folderCellHeightPx表示展开文件夹内图标竖向间距，可重新赋值；

                        3. ShortcutAndWidgetContainer类的layoutChild方法修改展开文件夹框上下边距：

                                     childLeft表示展开文件夹内展示的矩形框左边距，可重新赋值； 

                                     childTop表示展开文件夹内展示的矩形框上边距，可重新赋值；                      

###         2.8、文件夹图标9宫格

                        1. ClippedFolderIconLayoutRule类实现九宫格功能

                                     MAX\_NUM\_ITEMS\_IN\_PREVIEW变量表示显示几个缩略图标，改为9

                                     MIN\_SCALE变量表示缩略图标最小大小，改为0.22f

                                     MAX\_SCALE变量表示缩略图标最大大小，改为0.22f(不做伸缩)

                                     scaleForItem函数固定缩略图标大小：

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3c5540dc47ac44e095102b237702f35f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     getPosition函数计算缩略图图标排序，对函数进行重写：

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/339d9ff188ca467994b1e6918edc999e~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     computePreviewItemDrawingParams函数控制缩略图图标排序

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/434ee513f1b841399c0d24b00960f835~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        2. FolderGridOrganizer类对isItemInPreview函数进行重写

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/aa026e6e67ce4512b42bfa8dc1532fa8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         2.9、展开的文件夹图标名字显示，图标圆角            

                        1. 文件夹图标名字显示

                                    Folder类的onFinishInflate函数修改文字参数

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/808a6e708a0d462191668b46bdc56d79~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                    FolderIcon类的inflateIcon函数修改逻辑

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/69e0d320a24f41f485ef0c1d6e3bdebe~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        2. 文件夹图标圆角配置

                                    folder\_shapes.xml文件配置参数

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d1240f194c1643f59276a6d5fdf67a8d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp) 

                        3. 文件夹背景透明

                                  FolderAnimationManager类的构造方法屏蔽mFolderBackground赋值

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0a0050f590ea442d8626fa932c5a301f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                  FolderAnimationManager类的getAnimator函数屏蔽mFolderBackground赋值

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/36b01f5a82724cd9a1a13b25c708a61b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp) 

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8a875093001347f09c269afbdca33f55~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                  FolderIcon类的dispatchDraw函数屏蔽mBackground变量赋值

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/174288754f144c0b8bfa12af12e68d21~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)                           

###         3.0、展开的文件夹多个知识点修改                             

                       1. 多个分页文件夹名称居中

                                     FolderPagedView类的arrangeChildren函数修改逻辑

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d492a38323d74d7fa04e662a853e7e56~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        2. 靠左显示

                                     CellLayout类的onLayout函数修改逻辑

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/136b3b64f81f45c1a7b279dbfb9b494e~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        3. 窗口居中

                                     Folder类的centerAboutIcon函数lp.x和lp.y变量为矩形框坐标，参数说明

                                             720为屏幕分辨率的横向尺寸

                                             1600为屏幕分辨率的竖向尺寸

                                             24为状态栏尺寸

                                             48为导航栏尺寸

                                             ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4c88aa181df740a2b8fafa672c1a850d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                      Folder类的getContentAreaHeight函数固定矩形框高度

                                             ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/16ff9b18eb4e49298d995087fbbcbab6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                      FolderPagedView类的getDesiredWidth函数固定矩形框宽度

                                             ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/19670b0269464caf9bbed53cedfa9127~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                        4. 背景圆角

                                     PreviewBackground类的getScaledRadius函数修改圆角大小

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7bce71f2f00e4e0e95de950516ead203~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp) 

                        5. 图标固定模板网格显示（如3\*4）

                                     FolderGridOrganizer类对calculateGridSize函数进行从写，屏蔽原生逻辑

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8388f29da0854b5a90834b4cbc5b679f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                    此函数中添加新计算逻辑

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/36a16b4cdd784e64a35474d206409af7~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

###         3.1、长按卸载APP的修改，防卸载功能控制，图标白边

                        1. 长按卸载的修改

                                     DeleteDropTarget类对setTextBasedOnDragSource函数进行修改

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/762a814095bd46f7aca48c98130f8ad3~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp) 

                                     DeleteDropTarget类对setControlTypeBasedOnDragSource函数进行修改

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fb2c067aca834a18acc3dea47b82056b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     DeleteDropTarget类添加isCanDrop函数，提供判断

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5d35daa8566346f7aa4939febee818df~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     DeleteDropTarget类对completeDrop函数进行修改，屏蔽Snackbar.show   

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c52bb7d84087467ea36da1747be951a8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     DeleteDropTarget类对onAccessibilityDrop函数进行修改

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/894fad5f53d34944b08a06a960d4fedf~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     DragController类对drop函数进行修改

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2859339c94f6496ba90c8fd85ce80dfa~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                                     DragController类添加函数isNeedCancelDrag

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c8bf4a74e3a2435c92a4deeda93be17d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                         2. 防卸载功能控制

                                    SecondaryDropTarget类对supportsAccessibilityDrop函数进行修改

                                    ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/dee25f9feb4b4bddb674fa84b2f30c7c~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

                         3. 图标去白边 

                                    BaseIconFactory类对normalizeAndWrapToAdaptiveIcon函数进行修改

                                     ![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/61c0cec7ed0a40878032ec3d8ef383b7~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)                                                       

##  三、总结

        Launcher3的定制根据项目需求有各个点的修改，要随心所欲对其修改还需要理解其框架，以

        大的方向去看问题，角度不一样解决问题的思路会豁然开朗。后续还有更多关于Launcher3、

        原生Settings、SystemUI等系统应用以及framework端的博客，我主要做framework的开发工

        作，后续以framework博客为主，涉及的平台有rk、紫光展讯zr、全志、MTK、高通平台。

本文转自 [blog.csdn.net/CGTAIHYQ/ar…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2FCGTAIHYQ%2Farticle%2Fdetails%2F122047301%3Fspm%3D1001.2100.3001.7377%26utm_medium%3Ddistribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase%26depth_1-utm_source%3Ddistribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase "https://blog.csdn.net/CGTAIHYQ/article/details/122047301?spm=1001.2100.3001.7377&utm_medium=distribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase&depth_1-utm_source=distribute.pc_feed_blog_category.none-task-blog-classify_tag-3-122047301-null-null.nonecase")，如有侵权，请联系删除。