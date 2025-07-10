

桌面搜素栏属于桌面小组件wiget,简称QSB,默认在桌面launcher3初始化时加载，去掉的方法直接在launcer3中添加逻辑，在初始化时不加载视图，修改点：

```java
#packages/apps/Launcher3/src/com/android/launcher3/config/FeatureFlags.java
    /**
     * Enable moving the QSB on the 0th screen of the workspace. This is not a configuration feature
     * and should be modified at a project level.
     */
    public static final boolean QSB_ON_FIRST_SCREEN = false //设为false默认不加载
```

了解下上面的标志为作用的地方：

```java
    /**
     * Initializes and binds the first page
     * @param qsb an existing qsb to recycle or null.
     */
    public void bindAndInitFirstWorkspaceScreen(View qsb) {
        if (!FeatureFlags.QSB_ON_FIRST_SCREEN) {
            return; //false的时候直接返回了，不会加载视图
        }
        // Add the first page
        CellLayout firstPage = insertNewWorkspaceScreen(Workspace.FIRST_SCREEN_ID, 0);
        // Always add a QSB on the first screen.
        if (qsb == null) {
            // In transposed layout, we add the QSB in the Grid. As workspace does not touch the
            // edges, we do not need a full width QSB.
            qsb = LayoutInflater.from(getContext())
                    .inflate(R.layout.search_container_workspace, firstPage, false);
        }

        CellLayout.LayoutParams lp = new CellLayout.LayoutParams(0, 0, firstPage.getCountX(), 1);
        lp.canReorder = false;
        if (!firstPage.addViewToCellLayout(qsb, 0, R.id.search_container_workspace, lp, true)) {
            Log.e(TAG, "Failed to add to item at (0, 0) to CellLayout");
        }
    }
```