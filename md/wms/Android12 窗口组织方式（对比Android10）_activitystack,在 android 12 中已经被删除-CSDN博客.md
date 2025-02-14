## 1\. Android10窗口容器组织方式

### 1.1 [AMS](https://so.csdn.net/so/search?q=AMS&spm=1001.2101.3001.7020)和WMS

Android10上对窗口的组织方式有两部分，AMS和[WMS](https://so.csdn.net/so/search?q=WMS&spm=1001.2101.3001.7020)，AMS容器从大到小依次为：`ActivityDisplay`\->`ActivityStack`\->`TaskRecord`\->`ActivityRecord`，

WMS容器这边复杂很多，因为有一些特殊的窗口，大致可以理解为：顶级容器`DisplayContent`，其下有四大窗口容器，1. `mBelowAppWindowsContainers`（存储非APP类型窗口，并且要求其显示在APP之下，例如[壁纸](https://so.csdn.net/so/search?q=%E5%A3%81%E7%BA%B8&spm=1001.2101.3001.7020)），2. `mTaskStackContainers`（存储APP类型窗口），3. `mAboveAppWindowsContainers`（存储非APP类型窗口，并且要求其显示在APP之上，例如状态栏），4. `mImeWindowsContainers`（存储输入法窗口）。

除`mTaskStackContainers`之外，其他三大容器之下存储`WindowToken`，`WindowToken`之下为`WindowState`，`WindowState`之下还可以存储`WindowState`，而对于`mTaskStackContainers`，其内部存储`TaskStack`，`TaskStack`之下为`Task`，`Task`之下为`AppWindowToken`，`AppWindowToken`之下为`WindowState`，`WindowState`之下还可以存储`WindowState`。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a15657df870bdffadcd39987d5f2b639.png#pic_center)

AMS和WMS在应用窗口这块是有对应关系的，上图AMS部分`ActivityDisplay`对应WMS部分`DisplayContent`，`ActivityStack`对应`TaskStack`，`TaskRecord`对应`Task`，

`ActivityRecord`对应`AppWindowToken`，这些就是Android10上对窗口的组织方式。

## 2\. Android12窗口容器组织方式

### 2.1 窗口容器组织统一化

Android12相对于Android10，窗口容器这块发生了巨大变化（Android11其实就已经变了很多了），Android12上对于窗口组织不再划分AMS和WMS，而是统一采取一种组织方式，顶层的`Display`，之下引入一个叫`DisplayArea`的概念，`DisplayArea`用来描述`Display`上一块可以放置内容的显示区域，`DisplayArea`和Z-order强相关，理论上来说Z-order的每一层都可以创建一块`DisplayArea`，而`DisplayArea`作为容器其内部可以存储`DisplayArea`，`WindowToken`，`ActivityStack`（Android12上已经没有ActivityStack这个类，ActivityStack和Task统一由Task表示），`Task`内部存储`WindowToken`，`WindowToken`内部存储`WindowState`，`WindowState`内部还可以有`WindowState`。

相较于Android10，Android12上没有了`ActivityDisplay`，`ActivityStack`，`TaskRecord`，`TaskStack`，`AppWindowToken`这些概念，Android12上窗口容器组织方式大概为：`DisplayContent`\->`DisplayArea`\->`Task`\->`WindowToken`（Activity窗口为ActivityRecord）->`WindowState`，`DisplayContent`是`DisplayArea`的间接子类，`ActivityRecord`是`WindowToken`的子类，所有容器顶级抽象父类`ConfigurationContainer`，次级父类`WindowContainer`，这两个父类中提供了所有容器的通用逻辑，`ConfigurationContainer`中提供的是和窗口配置相关的信息（例如窗口尺寸），`WindowContainer`中提供的是子类作为窗口容器所具备的能力（例如增删，排序）。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/78d0186721e519c97ccd5d6f749b1ba0.png#pic_center)

### 2.2 DisplayArea分类

Android12上`DisplayArea`作为顶级容器，不同的`DisplayArea`显示能力是不同的，Android12是如何区分`DisplayArea`的呢，其实和Android10上`DisplayContent`下的四大容器类似，Android12将`DisplayArea`分成了多种不同的类型，将它们的能力称为`Feature`，`FeatureID`定义在`DisplayAreaOrganizer`中：

```
public class DisplayAreaOrganizer extends WindowOrganizer {
......
/**
     * The value in display area indicating that no value has been set.
     */
    public static final int FEATURE_UNDEFINED = -1;

    /**
     * The Root display area on a display
     */
    public static final int FEATURE_SYSTEM_FIRST = 0;

    /**
     * The Root display area on a display
     */
    public static final int FEATURE_ROOT = FEATURE_SYSTEM_FIRST;

    /**
     * Display area hosting the default task container.
     */
    public static final int FEATURE_DEFAULT_TASK_CONTAINER = FEATURE_SYSTEM_FIRST + 1;

    /**
     * Display area hosting non-activity window tokens.
     */
    public static final int FEATURE_WINDOW_TOKENS = FEATURE_SYSTEM_FIRST + 2;

    /**
     * Display area for one handed feature
     */
    public static final int FEATURE_ONE_HANDED = FEATURE_SYSTEM_FIRST + 3;
/**
     * Display area that can be magnified in
     * {@link Settings.Secure.ACCESSIBILITY_MAGNIFICATION_MODE_WINDOW}. It contains all windows
     * below {@link WindowManager.LayoutParams#TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY}.
     */
    public static final int FEATURE_WINDOWED_MAGNIFICATION = FEATURE_SYSTEM_FIRST + 4;

    /**
     * Display area that can be magnified in
     * {@link Settings.Secure.ACCESSIBILITY_MAGNIFICATION_MODE_FULLSCREEN}. This is different from
     * {@link #FEATURE_WINDOWED_MAGNIFICATION} that the whole display will be magnified.
     * @hide
     */
    public static final int FEATURE_FULLSCREEN_MAGNIFICATION = FEATURE_SYSTEM_FIRST + 5;

    /**
     * Display area for hiding display cutout feature
     * @hide
     */
    public static final int FEATURE_HIDE_DISPLAY_CUTOUT = FEATURE_SYSTEM_FIRST + 6;

    /**
     * Display area that the IME container can be placed in. Should be enabled on every root
     * hierarchy if IME container may be reparented to that hierarchy when the IME target changed.
     * @hide
     */
    public static final int FEATURE_IME_PLACEHOLDER = FEATURE_SYSTEM_FIRST + 7;

    /**
     * Display area for one handed background layer, which preventing when user
     * turning the Dark theme on, they can not clearly identify the screen has entered
     * one handed mode.
     * @hide
     */
    public static final int FEATURE_ONE_HANDED_BACKGROUND_PANEL = FEATURE_SYSTEM_FIRST + 8;

    /**
     * The last boundary of display area for system features
     */
    public static final int FEATURE_SYSTEM_LAST = 10_000;

    /**
     * Vendor specific display area definition can start with this value.
     */
    public static final int FEATURE_VENDOR_FIRST = FEATURE_SYSTEM_LAST + 1;

    /**
     * Last possible vendor specific display area id.
     * @hide
     */
    public static final int FEATURE_VENDOR_LAST = FEATURE_VENDOR_FIRST + 10_000;

    /**
     * Task display areas that can be created at runtime start with this value.
     * @see #createTaskDisplayArea(int, int, String)
     * @hide
     */
    public static final int FEATURE_RUNTIME_TASK_CONTAINER_FIRST = FEATURE_VENDOR_LAST + 1;

......

}
```

这里面定义了很多值，这些值和定义在`WindowManager.LayoutParams`中的窗口类型有异曲同工之妙，随便来看一个`FEATURE_DEFAULT_TASK_CONTAINER`：

```
  /**
     * Display area hosting the default task container.
     */
    public static final int FEATURE_DEFAULT_TASK_CONTAINER = FEATURE_SYSTEM_FIRST + 1;
```

很显然这个值定义的`DisplayArea`是用来存放Activity相关的窗口的，

再看一个`FEATURE_IME_PLACEHOLDER`，

```
 /**
     * Display area that the IME container can be placed in. Should be enabled on every root
     * hierarchy if IME container may be reparented to that hierarchy when the IME target changed.
     * @hide
     */
    public static final int FEATURE_IME_PLACEHOLDER = FEATURE_SYSTEM_FIRST + 7;
```

这个值定义的`DisplayArea`是用来存放输入法窗口的，所以我们明白了`DisplayArea`的大概划分方式。

### 2.3 DisplayArea对应Feature的创建及含义

Android12上对窗口的组织方式是以树形结构来组织的，屏幕初始化阶段由`DisplayAreaPolicyBuilder`的内部类`HierarchyBuilder`类构建，树的根节点为`RootDisplayArea`，其实例为`DisplayContent`实际指代的是一块具体的屏幕，

```
public DisplayAreaPolicy instantiate(WindowManagerService wmService,
                DisplayContent content, RootDisplayArea root,   //这里的content和root是同一个DisplayContent对象，指代一块屏幕
                DisplayArea.Tokens imeContainer) {
           //构建存储Activity类型窗口的TaskDisplayArea， "DefaultTaskDisplayArea"这是名字
            final TaskDisplayArea defaultTaskDisplayArea = new TaskDisplayArea(content, wmService,
                    "DefaultTaskDisplayArea", FEATURE_DEFAULT_TASK_CONTAINER);
            final List<TaskDisplayArea> tdaList = new ArrayList<>();
    
            tdaList.add(defaultTaskDisplayArea);

            //创建 树形结构的构建者，根节点root
            final HierarchyBuilder rootHierarchy = new HierarchyBuilder(root);
            // 将输入法区域和Activity窗口区域设置进去，这两个显示区域作为屏幕的基本功能，无论什么屏幕都应该有
            rootHierarchy.setImeContainer(imeContainer).setTaskDisplayAreas(tdaList);
            if (content.isTrusted()) {
                // 这里面会构建一些系统窗口的DisplayArea，如状态栏，导航栏等，所以有一个判断条件，必须当前屏幕是可信的
                configureTrustedHierarchyBuilder(rootHierarchy, wmService, content);
            }
            // 真正构建DisplayArea的地方，这里面会根据前面打开的DisplayAreas相关Feature来决定创建DisplayArea
            return new DisplayAreaPolicyBuilder().setRootHierarchy(rootHierarchy).build(wmService);
        }
```

从上述方法可以知道，`TaskDisplayArea`和`ImeContainer`是必不可少的屏幕区域，还有其他的屏幕区域的`Feature`会在屏幕可信时打开：

```
private void configureTrustedHierarchyBuilder(HierarchyBuilder rootHierarchy,
                WindowManagerService wmService, DisplayContent content) {
     
            rootHierarchy.addFeature(new Feature.Builder(wmService.mPolicy, "WindowedMagnification",
                    FEATURE_WINDOWED_MAGNIFICATION)
                    .upTo(TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY)
                    .except(TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY)
                    // Make the DA dimmable so that the magnify window also mirrors the dim layer.
                    .setNewDisplayAreaSupplier(DisplayArea.Dimmable::new)
                    .build());
    
           //默认屏幕即displayId = 0才会拥有下面的Feature
            if (content.isDefaultDisplay) {
              
                rootHierarchy.addFeature(new Feature.Builder(wmService.mPolicy, "HideDisplayCutout",
                        FEATURE_HIDE_DISPLAY_CUTOUT)
                        .all()
                        .except(TYPE_NAVIGATION_BAR, TYPE_NAVIGATION_BAR_PANEL, TYPE_STATUS_BAR,
                                TYPE_NOTIFICATION_SHADE)
                        .build())
                        .addFeature(new Feature.Builder(wmService.mPolicy,
                                "OneHandedBackgroundPanel",
                                FEATURE_ONE_HANDED_BACKGROUND_PANEL)
                                .upTo(TYPE_WALLPAPER)
                                .build())
                        .addFeature(new Feature.Builder(wmService.mPolicy, "OneHanded",
                                FEATURE_ONE_HANDED)
                                .all()
                                .except(TYPE_NAVIGATION_BAR, TYPE_NAVIGATION_BAR_PANEL)
                                .build());
            }
    
            rootHierarchy
                    .addFeature(new Feature.Builder(wmService.mPolicy, "FullscreenMagnification",
                            FEATURE_FULLSCREEN_MAGNIFICATION)
                            .all()
                            .except(TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY, TYPE_INPUT_METHOD,
                                    TYPE_INPUT_METHOD_DIALOG, TYPE_MAGNIFICATION_OVERLAY,
                                    TYPE_NAVIGATION_BAR, TYPE_NAVIGATION_BAR_PANEL)
                            .build())
                    .addFeature(new Feature.Builder(wmService.mPolicy, "ImePlaceholder",
                            FEATURE_IME_PLACEHOLDER)
                            .and(TYPE_INPUT_METHOD, TYPE_INPUT_METHOD_DIALOG)
                            .build());
        }
    }
```

上面代码中通过`new Feature.Builder`的方式创建`DisplayArea`的属性，包括名字，`FeatureId`（也可以理解成DisplayArea的类型），先来看看`Feature`这个类，`Feature`是`DisplayAreaPolicyBuilder`的内部类，很明显的建造者模式：

```
/**
     * A feature that requires {@link DisplayArea DisplayArea(s)}.
     */
    static class Feature {
        private final String mName;
        private final int mId;
        private final boolean[] mWindowLayers;
        private final NewDisplayAreaSupplier mNewDisplayAreaSupplier;

        private Feature(String name, int id, boolean[] windowLayers,
                NewDisplayAreaSupplier newDisplayAreaSupplier) {
            mName = name;
            mId = id;
            mWindowLayers = windowLayers;
            mNewDisplayAreaSupplier = newDisplayAreaSupplier;
        }

        /**
         * Returns the id of the feature.
         *
         * <p>Must be unique among the features added to a {@link DisplayAreaPolicyBuilder}.
         *
         * @see android.window.DisplayAreaOrganizer#FEATURE_SYSTEM_FIRST
         * @see android.window.DisplayAreaOrganizer#FEATURE_VENDOR_FIRST
         */
        public int getId() {
            return mId;
        }

        @Override
        public String toString() {
            return "Feature(\"" + mName + "\", " + mId + '}';
        }

        static class Builder {
            private final WindowManagerPolicy mPolicy;
            private final String mName;
            private final int mId;
            private final boolean[] mLayers;
            private NewDisplayAreaSupplier mNewDisplayAreaSupplier = DisplayArea::new;
            private boolean mExcludeRoundedCorner = true;

            /**
             * Builds a new feature that applies to a set of window types as specified by the
             * builder methods.
             *
             * <p>The set of types is updated iteratively in the order of the method invocations.
             * For example, {@code all().except(TYPE_STATUS_BAR)} expresses that a feature should
             * apply to all types except TYPE_STATUS_BAR.
             *
             * <p>The builder starts out with the feature not applying to any types.
             *
             * @param name the name of the feature.
             * @param id of the feature. {@see Feature#getId}
             */
            Builder(WindowManagerPolicy policy, String name, int id) {
                mPolicy = policy;
                mName = name;
                mId = id;
                mLayers = new boolean[mPolicy.getMaxWindowLayer() + 1];
            }

            /**
             * Set that the feature applies to all window types.
             */
            Builder all() {
                Arrays.fill(mLayers, true);
                return this;
            }

            /**
             * Set that the feature applies to the given window types.
             */
            Builder and(int... types) {
                for (int i = 0; i < types.length; i++) {
                    int type = types[i];
                    set(type, true);
                }
                return this;
            }

            /**
             * Set that the feature does not apply to the given window types.
             */
            Builder except(int... types) {
                for (int i = 0; i < types.length; i++) {
                    int type = types[i];
                    set(type, false);
                }
                return this;
            }

            /**
             * Set that the feature applies window types that are layerd at or below the layer of
             * the given window type.
             */
            Builder upTo(int typeInclusive) {
                final int max = layerFromType(typeInclusive, false);
                for (int i = 0; i < max; i++) {
                    mLayers[i] = true;
                }
                set(typeInclusive, true);
                return this;
            }

            /**
             * Sets the function to create new {@link DisplayArea} for this feature. By default, it
             * uses {@link DisplayArea}'s constructor.
             */
            Builder setNewDisplayAreaSupplier(NewDisplayAreaSupplier newDisplayAreaSupplier) {
                mNewDisplayAreaSupplier = newDisplayAreaSupplier;
                return this;
            }

            // TODO(b/155340867): consider to remove the logic after using pure Surface for rounded
            // corner overlay.
            Builder setExcludeRoundedCornerOverlay(boolean excludeRoundedCorner) {
                mExcludeRoundedCorner = excludeRoundedCorner;
                return this;
            }

            Feature build() {
                if (mExcludeRoundedCorner) {
                    // Always put the rounded corner layer to the top most layer.
                    mLayers[mPolicy.getMaxWindowLayer()] = false;
                }
                return new Feature(mName, mId, mLayers.clone(), mNewDisplayAreaSupplier);
            }

            private void set(int type, boolean value) {
                mLayers[layerFromType(type, true)] = value;
                if (type == TYPE_APPLICATION_OVERLAY) {
                    mLayers[layerFromType(type, true)] = value;
                    mLayers[layerFromType(TYPE_SYSTEM_ALERT, false)] = value;
                    mLayers[layerFromType(TYPE_SYSTEM_OVERLAY, false)] = value;
                    mLayers[layerFromType(TYPE_SYSTEM_ERROR, false)] = value;
                }
            }

            private int layerFromType(int type, boolean internalWindows) {
                return mPolicy.getWindowLayerFromTypeLw(type, internalWindows);
            }
        }
    }
```

`Feature`中包含几个重要的参数，`mName`，`mId`，`mWindowLayers`，名字和ID好理解，`mWindowLayers`的用处是啥？首先`mWindowLayers`数组长度为`mPolicy.getMaxWindowLayer()` + 1，`mPolicy.getMaxWindowLayer()`返回系统所有的窗口层数，固定值为36，我们都知道Android的窗口以Z-order决定显示层级，而Z-order又由窗口类型决定，Android系统定义的窗口类型能达到的最高层级为35，`mPolicy.getMaxWindowLayer()`定义为刚好比Z-order最大值大1，这块儿逻辑定义在`WindowManagerPolicy`中，结合`Feature`中的注释和代码，猜测`mWindowLayers`数组用来决定当前`Feature`对应的`DisplayArea`是否可以显示`mWindowLayers`数组下标所对应的窗口类型的窗口，例如`mWindowLayers[8]`为false，即此`DisplayArea`不能显示type等于8的窗口，即无法显示Toast：

```
 case TYPE_TOAST:
                return  8;
```

这个逻辑刚好符合`Feature.Builder`的`all`，`and`，`except`，`upTo`，`set`这些方法，另外，外部调用这几个方法时传递的参数不会是定义在`WindowManager.LayoutParams`中的窗口类型，这些窗口类型还需经过`mPolicy.getWindowLayerFromTypeLw`映射才能得到1，2，3，4…这种层级。

再回到前面创建`Feature`的方法`configureTrustedHierarchyBuilder`，随便看其中一个`Feature`的创建方式：

```
rootHierarchy.addFeature(new Feature.Builder(wmService.mPolicy, "WindowedMagnification",
                    FEATURE_WINDOWED_MAGNIFICATION)
                    .upTo(TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY)
                    .except(TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY)
                    // Make the DA dimmable so that the magnify window also mirrors the dim layer.
                    .setNewDisplayAreaSupplier(DisplayArea.Dimmable::new)
                    .build());
```

现在再看这个`Feature`的创建就明了多了，意思是创建一个名字为`"WindowedMagnification"`，类型为`FEATURE_WINDOWED_MAGNIFICATION`，最多只能显示小于窗口类型为`TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY`的层级的窗口，且不包含`TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY`的`DisplayArea`的`Feature`，`TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY`对应窗口层级为32。

再看一个输入法的：

```
.addFeature(new Feature.Builder(wmService.mPolicy, "ImePlaceholder",
                            FEATURE_IME_PLACEHOLDER)
                            .and(TYPE_INPUT_METHOD, TYPE_INPUT_METHOD_DIALOG)
                            .build());
```

这里输入法的`DisplayArea`的`Feature`仅能显示窗口类型为`TYPE_INPUT_METHOD`和`TYPE_INPUT_METHOD_DIALOG`所对应的层级的窗口，其实就是只能显示输入法窗口。

### 2.4 DisplayArea树形结构的构建

上一节看到了`HierarchyBuilder`是`DisplayArea`树形结构的构建者，其内部保存了树形结构的根节点`RootDisplayArea`，其实例为`DisplayContent`对应一块屏幕，并且不同的`DisplayArea`还创建了对应的`Feature`，这些`Feature`也被保存在了`HierarchyBuilder`内部，最后会调用`HierarchyBuilder`的`build`方法来构建`DisplayArea`树：

```
private void build(@Nullable List<HierarchyBuilder> displayAreaGroupHierarchyBuilders) {
            final WindowManagerPolicy policy = mRoot.mWmService.mPolicy;
            //maxWindowLayerCount = 37
            final int maxWindowLayerCount = policy.getMaxWindowLayer() + 1;
            final DisplayArea.Tokens[] displayAreaForLayer =
                    new DisplayArea.Tokens[maxWindowLayerCount];
            //一个Feature可以对应多个DisplayArea
            final Map<Feature, List<DisplayArea<WindowContainer>>> featureAreas =
                    new ArrayMap<>(mFeatures.size());
            for (int i = 0; i < mFeatures.size(); i++) {
                featureAreas.put(mFeatures.get(i), new ArrayList<>());
            }

            //PendingArea用来创建DisplayArea
            PendingArea[] areaForLayer = new PendingArea[maxWindowLayerCount];
            //作为areaForLayer的初始默认值
            final PendingArea root = new PendingArea(null, 0, null);
            Arrays.fill(areaForLayer, root);

           
            /*
            下面这个循环里面是对DisplayArea创建的规则的定义，包括其父DisplayArea，子DisplayArea的定义规则，
            规则的主要制定由feature.mWindowLayers这个数组决定，
            为了简便，假设，feature.mWindowLayers数组长度为5，以如下三种类型feature.mWindowLayers为例：
            
            1. [true,true,true,false,false]，2. [true,true,false,true,true]，3. [false,false,true,true,false]
            areaForLayer初始值为：[PendingArea(root)(mChildren = null,mParent = null),
            PendingArea(root)(mChildren = null,mParent = null),
            PendingArea(root)(mChildren = null,mParent = null),
            PendingArea(root)(mChildren = null,mParent = null),
            PendingArea(root)(mChildren = null,mParent = null)],
            
            第一个feature循环完之后：
            areaForLayer值为：[PendingArea(1)(mChildren = null,mParent = PendingArea(root)),
            PendingArea(1)(mChildren = null,mParent = PendingArea(root)),
            PendingArea(1)(mChildren = null,mParent = PendingArea(root)),
            PendingArea(root)(mChildren = PendingArea(1),mParent = null),
            PendingArea(root)(mChildren = PendingArea(1),mParent = null)],
            
            第二个feature循环完之后：
                 areaForLayer值为：[PendingArea(2)(mChildren = null,mParent = PendingArea(1)),
            PendingArea(2)(mChildren = null,mParent = PendingArea(1)),
            PendingArea(1)(mChildren = PendingArea(2),mParent = PendingArea(root)),
            PendingArea(3)(mChildren = null,mParent = PendingArea(root)),
                PendingArea(3)(mChildren = null,mParent = PendingArea(root))],
                
            此时：PendingArea(root)(mChildren = (PendingArea(1),PendingArea(3)))
            
          第三个个feature循环完之后：[false,false,true,true,false]
            areaForLayer值为：[PendingArea(2)(mChildren = null,mParent = PendingArea(1)),
            PendingArea(2)(mChildren = null,mParent = PendingArea(1)),
            PendingArea(4)(mChildren = null,mParent = PendingArea(1)),
            PendingArea(5)(mChildren = null,mParent = PendingArea(3)),
                PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root))],
                
            此时：PendingArea(1)(mChildren = (PendingArea(2),PendingArea(4)),mParent = PendingArea(root))
            
            */
           //（1）
            final int size = mFeatures.size();
            for (int i = 0; i < size; i++) {
                
                final Feature feature = mFeatures.get(i);
                PendingArea featureArea = null;
                for (int layer = 0; layer < maxWindowLayerCount; layer++) {
                    if (feature.mWindowLayers[layer]) {
                       
                        if (featureArea == null || featureArea.mParent != areaForLayer[layer]) {
                           
                            featureArea = new PendingArea(feature, layer, areaForLayer[layer]);
                            areaForLayer[layer].mChildren.add(featureArea);
                        }
                        areaForLayer[layer] = featureArea;
                    } else {
                     
                        featureArea = null;
                    }
                }
            }
    
    

            //（2）
            PendingArea leafArea = null;
            int leafType = LEAF_TYPE_TOKENS;
            for (int layer = 0; layer < maxWindowLayerCount; layer++) {
                int type = typeOfLayer(policy, layer);
                
                if (leafArea == null || leafArea.mParent != areaForLayer[layer]
                        || type != leafType) {
                    leafArea = new PendingArea(null /* feature */, layer, areaForLayer[layer]);
                    areaForLayer[layer].mChildren.add(leafArea);
                    leafType = type;
                    if (leafType == LEAF_TYPE_TASK_CONTAINERS) {
                        
                        addTaskDisplayAreasToApplicationLayer(areaForLayer[layer]);
                        addDisplayAreaGroupsToApplicationLayer(areaForLayer[layer],
                                displayAreaGroupHierarchyBuilders);
                        leafArea.mSkipTokens = true;
                    } else if (leafType == LEAF_TYPE_IME_CONTAINERS) {
                        
                        leafArea.mExisting = mImeContainer;
                        leafArea.mSkipTokens = true;
                    }
                }
                leafArea.mMaxLayer = layer;
            }
    
            //（3）
            root.computeMaxLayer();

            //（4）
            root.instantiateChildren(mRoot, displayAreaForLayer, 0, featureAreas);

            //（5）
            mRoot.onHierarchyBuilt(mFeatures, displayAreaForLayer, featureAreas);
        }
```

上述计算`DisplayArea`树的算法比较复杂，分五个步骤解析。

#### 2.4.1 步骤1

步骤1的循环里面是对`DisplayArea`创建规则的定义，包括其父`DisplayArea`，子`DisplayArea`的定义规则，规则的主要制定由`feature.mWindowLayer`s这个数组决定，为了简便，假设，`feature.mWindowLayers`数组长度为5，以如下三种类型`feature.mWindowLayers`为例：

1.  \[true,true,true,false,false\]，2. \[true,true,false,true,true\]，3. \[false,false,true,true,false\]

```
 
           //（1）
            final int size = mFeatures.size();
            for (int i = 0; i < size; i++) {
                
                final Feature feature = mFeatures.get(i);
                PendingArea featureArea = null;
                for (int layer = 0; layer < maxWindowLayerCount; layer++) {
                    if (feature.mWindowLayers[layer]) {
                       
                        if (featureArea == null || featureArea.mParent != areaForLayer[layer]) {
                           
                            featureArea = new PendingArea(feature, layer, areaForLayer[layer]);
                            areaForLayer[layer].mChildren.add(featureArea);
                        }
                        areaForLayer[layer] = featureArea;
                    } else {
                     
                        featureArea = null;
                    }
                }
            }
/*
            areaForLayer初始值为：[PendingArea(root)(mChildren = null,mParent = null,mFeature = null),
            PendingArea(root)(mChildren = null,mParent = null,mFeature = null),
            PendingArea(root)(mChildren = null,mParent = null,mFeature = null),
            PendingArea(root)(mChildren = null,mParent = null,mFeature = null),
            PendingArea(root)(mChildren = null,mParent = null,mFeature = null)],
            
            第一个feature循环完之后：
            areaForLayer值为：[PendingArea(1)(mChildren = null,mParent = PendingArea(root),mFeature = feature1),
            PendingArea(1)(mChildren = null,mParent = PendingArea(root),mFeature = feature1),
            PendingArea(1)(mChildren = null,mParent = PendingArea(root),mFeature = feature1),
            PendingArea(root)(mChildren = PendingArea(1),mParent = null),
            PendingArea(root)(mChildren = PendingArea(1),mParent = null)],
            
            第二个feature循环完之后：
               areaForLayer值为：[PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
              PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
              PendingArea(1)(mChildren = PendingArea(2),mParent = PendingArea(root),mFeature = feature1),
              PendingArea(3)(mChildren = null,mParent = PendingArea(root),mFeature = feature2),
              PendingArea(3)(mChildren = null,mParent = PendingArea(root),mFeature = feature2)],
                
            此时：PendingArea(root)(mChildren = (PendingArea(1),PendingArea(3)),mParent = null,mFeature = null)
            
          第三个个feature循环完之后：[false,false,true,true,false]
            areaForLayer值为：[PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
            PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
            PendingArea(4)(mChildren = null,mParent = PendingArea(1),mFeature = feature3),
            PendingArea(5)(mChildren = null,mParent = PendingArea(3),mFeature = feature3),
                PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
                
            此时：PendingArea(1)(mChildren = (PendingArea(2),PendingArea(4)),mParent = PendingArea(root),mFeature = feature1)
            */

```

步骤1执行完之后可以得到一个初步树形结构，根为`PendingArea(root)`，  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b235ae2ceeffebeec7bc6fc645ca288e.png#pic_center)

#### 2.4.2 步骤2

```
    //（2）
            PendingArea leafArea = null;
             //HierarchyBuilder中定义了三种type，用来表示DisplayArea显示的窗口的类型，应用类型，输入法类型，其他类型
            int leafType = LEAF_TYPE_TOKENS;
            for (int layer = 0; layer < maxWindowLayerCount; layer++) {
                
                //每一层layer的窗口都属于一种HierarchyBuilder中定义的类型，typeOfLayer中定义的三种类型分别对应的layer为：
                //应用窗口（2），输入法（15，16），其他（除2，15，16之外的数字）
                int type = typeOfLayer(policy, layer);
                
                if (leafArea == null || leafArea.mParent != areaForLayer[layer]
                        || type != leafType) {
                    leafArea = new PendingArea(null /* feature */, layer, areaForLayer[layer]);
                    areaForLayer[layer].mChildren.add(leafArea);
                    leafType = type;
                    
                    if (leafType == LEAF_TYPE_TASK_CONTAINERS) {
                        //应用窗口的情况
                        addTaskDisplayAreasToApplicationLayer(areaForLayer[layer]);
                        addDisplayAreaGroupsToApplicationLayer(areaForLayer[layer],
                                displayAreaGroupHierarchyBuilders);
                        leafArea.mSkipTokens = true;
                    } else if (leafType == LEAF_TYPE_IME_CONTAINERS) {
                        //输入法窗口的情况
                        leafArea.mExisting = mImeContainer;
                        leafArea.mSkipTokens = true;
                    }
                }
                leafArea.mMaxLayer = layer;
            }

/*
同样以步骤1中的例子来解析步骤2的算法：
areaForLayer值为：[PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
            PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
            PendingArea(4)(mChildren = null,mParent = PendingArea(1),mFeature = feature3),
            PendingArea(5)(mChildren = null,mParent = PendingArea(3),mFeature = feature3),
            PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
                
  假设maxWindowLayerCount为5：
  第一次循环，layer = 0：应用窗口（layer = 2），输入法（layer = 15 || 16），其他（除2，15，16之外的layer）
  leafArea = PendingArea(6)(mChildren == null,mParent = PendingArea(2),mFeature = null)
  
  areaForLayer值为：[PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
            PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
            PendingArea(4)(mChildren = null,mParent = PendingArea(1),mFeature = feature3),
            PendingArea(5)(mChildren = null,mParent = PendingArea(3),mFeature = feature3),
            PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
            
 第二次循环，layer = 1：应用窗口（layer = 2），输入法（layer = 15 || 16），其他（除2，15，16之外的layer）
  leafArea = PendingArea(6)(mChildren == null,mParent = PendingArea(2),mFeature = null)
  
  areaForLayer值为：[PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
            PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
            PendingArea(4)(mChildren = null,mParent = PendingArea(1),mFeature = feature3),
            PendingArea(5)(mChildren = null,mParent = PendingArea(3),mFeature = feature3),
            PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
            
  第三次循环，layer = 2：应用窗口（layer = 2），输入法（layer = 15 || 16），其他（除2，15，16之外的layer）
    leafArea = PendingArea(7)(mChildren == null,mParent = PendingArea(4),mFeature = null)
  
  areaForLayer值为：[PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
            PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
            PendingArea(4)(mChildren = PendingArea(7),mParent = PendingArea(1),mFeature = feature3),
            PendingArea(5)(mChildren = null,mParent = PendingArea(3),mFeature = feature3),
            PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
            
                因为此时layer的类型为应用窗口，还会调addTaskDisplayAreasToApplicationLayer方法，最终
                
   areaForLayer值为：[PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
             PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
             PendingArea(4)(mChildren = (PendingArea(7),PendingArea(8)),mParent = PendingArea(1),mFeature = feature3),
             PendingArea(5)(mChildren = null,mParent = PendingArea(3),mFeature = feature3),
             PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
*/
            
private void addTaskDisplayAreasToApplicationLayer(PendingArea parentPendingArea) {
            final int count = mTaskDisplayAreas.size();
            for (int i = 0; i < count; i++) {
                PendingArea leafArea =
                        new PendingArea(null /* feature */, APPLICATION_LAYER, parentPendingArea);
                leafArea.mExisting = mTaskDisplayAreas.get(i);
                leafArea.mMaxLayer = APPLICATION_LAYER;
                parentPendingArea.mChildren.add(leafArea);
            }
        }
/*
   第四次循环，layer = 3：应用窗口（layer = 2），输入法（layer = 15 || 16），其他（除2，15，16之外的layer）
                        leafArea = PendingArea(9)(mChildren == null,mParent = PendingArea(5),mFeature = null)
                        
   areaForLayer值为：[PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
             PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
             PendingArea(4)(mChildren = (PendingArea(7),PendingArea(8)),mParent = PendingArea(1),mFeature = feature3),
             PendingArea(5)(mChildren = PendingArea(9),mParent = PendingArea(3),mFeature = feature3),
             PendingArea(3)(mChildren = PendingArea(5),mParent = PendingArea(root),mFeature = feature2)],
             
     第五次循环，layer = 4：应用窗口（layer = 2），输入法（layer = 15 || 16），其他（除2，15，16之外的layer）
                           leafArea = PendingArea(10)(mChildren == null,mParent = PendingArea(3),mFeature = null)
      
   areaForLayer值为：[PendingArea(2)(mChildren = PendingArea(6),mParent = PendingArea(1),mFeature = feature2),
             PendingArea(2)(mChildren = null,mParent = PendingArea(1),mFeature = feature2),
             PendingArea(4)(mChildren = (PendingArea(7),PendingArea(8)),mParent = PendingArea(1),mFeature = feature3),
             PendingArea(5)(mChildren = PendingArea(9),mParent = PendingArea(3),mFeature = feature3),
             PendingArea(3)(mChildren = (PendingArea(5),PendingArea(10)),mParent = PendingArea(root),mFeature = feature2)],
                        
*/
```

步骤2中创建了几个`mFeature = null`的`PendingArea`，然后挂在了对应的父节点下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d29b221e1f3dff0ce4b15fad18a5a3cd.png#pic_center)

#### 2.4.3 步骤3

步骤3调用`root.computeMaxLayer`方法作用是递归遍历树的所有节点，得到根节点的`mMaxLayer`，

```
int computeMaxLayer() {
            for (int i = 0; i < mChildren.size(); i++) {
                mMaxLayer = Math.max(mMaxLayer, mChildren.get(i).computeMaxLayer());
            }
            return mMaxLayer;
        }
```

#### 2.4.4 步骤4

步骤4会根据前面构建的`PendingArea`树来构建真正的`DisplayArea`树：

```
//这里的parent是DisplayArea树的容器
void instantiateChildren(DisplayArea<DisplayArea> parent, DisplayArea.Tokens[] areaForLayer,
                int level, Map<Feature, List<DisplayArea<WindowContainer>>> areas) {
          //根据mMinLayer从小到大排序
            mChildren.sort(Comparator.comparingInt(pendingArea -> pendingArea.mMinLayer));
            for (int i = 0; i < mChildren.size(); i++) {
                final PendingArea child = mChildren.get(i);
                //创建DisplayArea的核心操作
                final DisplayArea area = child.createArea(parent, areaForLayer);
                if (area == null) {
                    // TaskDisplayArea and ImeContainer can be set at different hierarchy, so it can
                    // be null.
                    continue;
                }
                //所有的DisplayArea都会被添加到DisplayContent中，顺序是后加入的在尾部
                parent.addChild(area, WindowContainer.POSITION_TOP);
                if (child.mFeature != null) {
                    //根据Feature对DisplayArea进行分类
                    areas.get(child.mFeature).add(area);
                }
                //递归
                child.instantiateChildren(area, areaForLayer, level + 1, areas);
            }
        }
```

主要来看`PendingArea.createArea`方法

```
@Nullable
        private DisplayArea createArea(DisplayArea<DisplayArea> parent,
                DisplayArea.Tokens[] areaForLayer) {
            if (mExisting != null) {
                if (mExisting.asTokens() != null) {
                    // Store the WindowToken container for layers
                    fillAreaForLayers(mExisting.asTokens(), areaForLayer);
                }
                return mExisting;
            }
            if (mSkipTokens) {
                return null;
            }
            DisplayArea.Type type;
            //三种类型的DisplayArea.Type，应用之上，应用之下，已经应用本身
            if (mMinLayer > APPLICATION_LAYER) {
                type = DisplayArea.Type.ABOVE_TASKS;
            } else if (mMaxLayer < APPLICATION_LAYER) {
                type = DisplayArea.Type.BELOW_TASKS;
            } else {
                type = DisplayArea.Type.ANY;
            }
            if (mFeature == null) {
                //对于mFeature为空的PendingArea，直接创建名为"Leaf"的DisplayArea
                final DisplayArea.Tokens leaf = new DisplayArea.Tokens(parent.mWmService, type,
                        "Leaf:" + mMinLayer + ":" + mMaxLayer);
                fillAreaForLayers(leaf, areaForLayer);
                return leaf;
            } else {
                //否则以mFeature.mName命名
                return mFeature.mNewDisplayAreaSupplier.create(parent.mWmService, type,
                        mFeature.mName + ":" + mMinLayer + ":" + mMaxLayer, mFeature.mId);
            }
        }
```

创建`DisplayArea`的方式很简单，调用构造函数就行了，上述`DisplayArea`大致分为两类，有`Feature`的会按照添加`Feature`时定义的名字命名，没有`Feature`的统一命名为 `“Leaf”`，当步骤4完成之后`DisplayArea`的树形结构就完成了，还有两个比较重要的点就是`DisplayArea`的`mMinLayer`和`mMaxLayer`，这两个值代表了`DisplayArea`所能显示的`layer`的范围。

还是按照步骤1，2所得的树形结构举例，最终得到的树如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c5e02b29060a6cfd521644b1925a90e7.png#pic_center)

feature1，feature2，feature3在实际代码中会以如下名字代替：

“WindowedMagnification”,“HideDisplayCutout”,“OneHandedBackgroundPanel”,“OneHanded”,"FullscreenMagnification"等。

再看看实际dump出来的结构：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6730f4259570add3b545715555fde840.png)

这是一张副屏幕的窗口结构图，基本和前面分析的树形结构相似。

在`DisplayArea`之下就是`Task`（Activity窗口）和`WindowToken`（非Activity窗口）的概念了，最底层是`WindowState`。

整个Android12的窗口组织方式相较于Android10发生了非常大的变化，可以说变得更简洁了。