# Jetpack



[TOC]



Jetpack中的Paging组件可以轻松的给RecyclerView增加分页加载的功能，通过预加载实现无限滑动的效果。

先说一下这无限滑动效果，项目中的分页加载一般分两大种情况：

1. 一种是滑到底部上拉松手后加载或者滑到底部后自动显示一个加载动画加载。
2. 一种是当还没滑动到底部的时候就开始加载了，当滑到底部的时候数据可能已经加载完成并续上了，这样就有一种无限滑动的感觉，Paging默认就是这种模式。

### 1 简单例子

下面先写个简单的小例子看看怎么用，然后在去看原理，使用鸿神的[玩安卓网站](https://link.juejin.cn/?target=https%3A%2F%2Fwanandroid.com%2Fblog%2Fshow%2F2)的首页内容的api来完成这个小例子

引入最新的依赖：

```
implementation "androidx.paging:paging-runtime:2.1.0"
复制代码
```

想要完成一个分页加载的列表，一般写三个部分，1. 数据部分 2.adapter部分 3.activity/fragment

Paging使用的时候当然要配合JetPack组件中的其他组件来使用了，用起来更酸爽，单独使用它没有意义。这里就配合LiveData和ViewModel

#### 1.1 数据来源

```
public class PagingViewModel extends ViewModel {

    private static final String TAG = PagingViewModel.class.getSimpleName();

    private LiveData<PagedList<ArticleResponse.DataBean.DatasBean>> articleRes = null;
    private ArticleDataSource mDataSource;
    //是否有数据
    private MutableLiveData<Boolean> boundaryPageData = new MutableLiveData<>();

    public LiveData<PagedList<ArticleResponse.DataBean.DatasBean>> getArticleLiveData() {
        if (articleRes == null) {
            PagedList.Config config = new PagedList.Config.Builder()
                    .setPageSize(20)
                    .setInitialLoadSizeHint(22)
                    .build();
            articleRes = new LivePagedListBuilder<Integer, ArticleResponse.DataBean.DatasBean>(mFactory, config)
                    .setBoundaryCallback(mBoundaryCallback)
                    .build();
        }
        return articleRes;
    }

    private DataSource.Factory mFactory = new DataSource.Factory() {
        @NonNull
        @Override
        public DataSource create() {
            if (mDataSource == null || mDataSource.isInvalid()) {
                mDataSource = new ArticleDataSource();
            }
            return mDataSource;
        }
    };

    //监听数据边界
    private PagedList.BoundaryCallback mBoundaryCallback = new PagedList.BoundaryCallback<ArticleResponse.DataBean.DatasBean>() {
        @Override
        public void onZeroItemsLoaded() {
            super.onZeroItemsLoaded();
            //初始化数据
            boundaryPageData.postValue(false);
        }

        @Override
        public void onItemAtFrontLoaded(@NonNull ArticleResponse.DataBean.DatasBean itemAtFront) {
            super.onItemAtFrontLoaded(itemAtFront);
            //正在添加数据
            boundaryPageData.postValue(true);
        }

        @Override
        public void onItemAtEndLoaded(@NonNull ArticleResponse.DataBean.DatasBean itemAtEnd) {
            super.onItemAtEndLoaded(itemAtEnd);
            //没有数据加载了
            boundaryPageData.postValue(false);
        }
    };

    public ArticleDataSource getDataSource() {
        return mDataSource;
    }

    public MutableLiveData<Boolean> getBoundaryPageData() {
        return boundaryPageData;
    }

    public void loadData(int currentPage, PageKeyedDataSource.LoadInitialCallback<Integer, ArticleResponse.DataBean.DatasBean> initialCallback
            , PageKeyedDataSource.LoadCallback<Integer, ArticleResponse.DataBean.DatasBean> callback) {
        String url = "https://www.wanandroid.com/article/list/" + currentPage + "/json";
        OkGo.<String>get(url)
                .execute(new StringCallback() {
                    @Override
                    public void onSuccess(Response<String> response) {
                        Gson gson = new Gson();
                        ArticleResponse articleResponse = gson.fromJson(response.body(), ArticleResponse.class);
                        if (initialCallback != null) {
                            initialCallback.onResult(articleResponse.getData().getDatas(), -1, 0);
                        } else {
                            callback.onResult(articleResponse.getData().getDatas(), currentPage);
                        }
                        boundaryPageData.postValue(articleResponse.getData().getDatas().size() <= 0);
                    }
                });
    }

    public class ArticleDataSource extends PageKeyedDataSource<Integer, ArticleResponse.DataBean.DatasBean> {
        @Override
        public void loadInitial(@NonNull LoadInitialParams<Integer> params, @NonNull LoadInitialCallback<Integer, ArticleResponse.DataBean.DatasBean> callback) {
            //开始加载数据
            loadData(0, callback, null);
            Log.d(TAG, "loadInitial");
        }

        @Override
        public void loadBefore(@NonNull LoadParams<Integer> params, @NonNull LoadCallback<Integer, ArticleResponse.DataBean.DatasBean> callback) {
            //往前加载数据
        }

        @Override
        public void loadAfter(@NonNull LoadParams<Integer> params, @NonNull LoadCallback<Integer, ArticleResponse.DataBean.DatasBean> callback) {
            //往后加载数据
            loadData(params.key + 1, null, callback);
            Log.d(TAG, "loadAfter");
        }
    }
}
复制代码
```

前面的代码主要就是干了两件事：

1. 如何创建一个数据集
2. 如何请求网络获取数据给数据集

数据集使用LiveData观察，使用**PagedList**保存，PagedList顾名思义页面集合或者说是数据集合，配合LiveData观察者可以很方便的增加数据。

创建数据集使用**LivePagedListBuilder**来创建，它需要两个参数**数据工厂和分页配置**

分页配置：通过**PagedList.Config**类来实现，可以通过构建者来给它设置不同的属性

- setPageSize() 设置每次分页加载的数量
- setInitialLoadSizeHint() 设置初始化数据的时候加载数据的数量
- setPrefetchDistance() 指定提前预加载的时机（跟最后一条的距离）
- setMaxSize() 指定数据源最大可以加载的数量
- setEnablePlaceholders() 是否使用占位符，配合setMaxSize()使用，未加载出来的部分使用占位符代替

数据工厂：**DataSource.Factory**主要是用来创建数据源 **DataSource** ，Paging框架主要提供了3种数据源类型。

- PageKeyedDataSource ：主要用于使用页码分页的情况每加载一次page++，上面的小demo就是使用的这种数据源
- ItemKeyedDataSource：下一页的加载需要前面的某个item的信息来加载。比如传入某个item的id，通过这个id来获取该item后面的数据
- PositionalDataSource ：数据源固定，通过特定位置来加载数据

demo中用的是PageKeyedDataSource，它是个抽象类，有三个抽象方法

1. loadInitial：初始化第一页数据
2. loadBefore：往前分页加载
3. loadAfter：往后分页加载

在loadInitial方法中开始第一页的数据，在loadAfter中开始往后分页加载，数据加载完成后保存在PagedList中

上面代码中还设置了一个**PagedList.BoundaryCallback**，它是数据加载的边界回调，它有三个回到方法分别是初始化数据，正在添加数据，数据没有了加载结束。

#### 1.2 adapter

```
public class PagingAdapter extends PagedListAdapter<ArticleResponse.DataBean.DatasBean, PagingAdapter.ViewHolder> {

    protected PagingAdapter() {
        super(new DiffUtil.ItemCallback<ArticleResponse.DataBean.DatasBean>() {
            @Override
            public boolean areItemsTheSame(@NonNull ArticleResponse.DataBean.DatasBean oldItem, @NonNull ArticleResponse.DataBean.DatasBean newItem) {
                return oldItem.getId() == newItem.getId();
            }

            @Override
            public boolean areContentsTheSame(@NonNull ArticleResponse.DataBean.DatasBean oldItem, @NonNull ArticleResponse.DataBean.DatasBean newItem) {
                return oldItem.equals(newItem);
            }
        });
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.list_item,parent,false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        ArticleResponse.DataBean.DatasBean bean = getItem(position);
          holder.tvname.setText(bean.getTitle());
    }

    public class ViewHolder extends RecyclerView.ViewHolder {
        TextView tvname;
        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            tvname = itemView.findViewById(R.id.tvname);
        }
    }
}
复制代码
```

使用Paging框架就必须要继承PagedListAdapter了，它强制要求传入一个DiffUtil.ItemCallback，它是用来对新旧数据之间进行差分计算，这样Paging框架就有差量更新的能力了。DiffUtil.ItemCallback的两个实现方法中我们需要自己定义一下差分规则。

剩下的就是跟正常写一个RecyclerView.Adapter一样了。onBindViewHolder中通过PagedListAdapter提供的`getItem(position);`方法拿到当前item的数据对象

#### 1.3 activity中使用

```
public class PagingActivity extends AppCompatActivity {
    private SmartRefreshLayout refreshLayout;
    private RecyclerView recyclerView;
    private PagingAdapter mAdapter;
    private PagingViewModel viewModel;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_paging);
        refreshLayout = findViewById(R.id.refreshLayout);
        recyclerView = findViewById(R.id.recyclerView);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        recyclerView.addItemDecoration(new DividerItemDecoration(this,DividerItemDecoration.VERTICAL));

        mAdapter  = new PagingAdapter();
        recyclerView.setAdapter(mAdapter);

        viewModel = new ViewModelProvider(this).get(PagingViewModel.class);
        viewModel.getArticleLiveData().observe(this, new Observer<PagedList<ArticleResponse.DataBean.DatasBean>>() {
            @Override
            public void onChanged(PagedList<ArticleResponse.DataBean.DatasBean> datasBeans) {
                   mAdapter.submitList(datasBeans);
            }
        });
        viewModel.getBoundaryPageData().observe(this, new Observer<Boolean>() {
            @Override
            public void onChanged(Boolean haData) {
                if(!haData){
                    refreshLayout.finishLoadMore();
                    refreshLayout.finishRefresh();
                }
            }
        });
    }
}
复制代码
```

通过viewModel.getArticleLiveData()方法拿到前面定义的LiveData然后调用它的observe方法，就能监听到数据的改变了，在其回调中调用adapter的submitList方法将返回的数据提交到adapter中，就能自动刷新加载了。

OK 到这里一个简单的Paging使用就完成了下面看看效果如何，这个接口每次默认返回20条数据。



![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/2/10/1702e1ad92288fe1~tplv-t2oaga2asx-watermark.awebp)



### 2 原理分析

知道怎么用了只是第一步，还需要去看看它的源码到底是怎么个流程，毕竟这只是一个通用框架，跟业务结合的时候保不准哪里就不吻合，需要自己改造一下。或者用的时候发现该框架有什么地方不合理，比如Paging框架如果有一次分页失败，就不继续分页了，如果是接口的问题，此时我们肯定希望能继续分页。

#### 2.1 如何初始化第一次加载的

下面看看主干流程，从Activity中开始看源码：

前面我们知道，调用 viewModel.getArticleLiveData()方法拿到一个LiveData对象，调用其observe方法就能监听到数据的改变了。这个LiveData对象是通过LivePagedListBuilder这个类build出来的。那就从这个build方法开始。

```
public LiveData<PagedList<Value>> build() {
    return create(mInitialLoadKey, mConfig, mBoundaryCallback, mDataSourceFactory,
        ArchTaskExecutor.getMainThreadExecutor(), mFetchExecutor);
    }
复制代码
```

调用了create方法，传入的参数我们也都很熟悉，有我们自定义的PagedList的配置，数据源工厂，还有线程池等

```
   private static <Key, Value> LiveData<PagedList<Value>> create(
            @Nullable final Key initialLoadKey,
            @NonNull final PagedList.Config config,
            @Nullable final PagedList.BoundaryCallback boundaryCallback,
            @NonNull final DataSource.Factory<Key, Value> dataSourceFactory,
            @NonNull final Executor notifyExecutor,
            @NonNull final Executor fetchExecutor) {
        return new ComputableLiveData<PagedList<Value>>(fetchExecutor) {
            @Nullable
            private PagedList<Value> mList;
            @Nullable
            private DataSource<Key, Value> mDataSource;

            private final DataSource.InvalidatedCallback mCallback =
                    new DataSource.InvalidatedCallback() {
                        @Override
                        public void onInvalidated() {
                            invalidate();
                        }
                    };

            @SuppressWarnings("unchecked") // for casting getLastKey to Key
            @Override
            protected PagedList<Value> compute() {
                @Nullable Key initializeKey = initialLoadKey;
                if (mList != null) {
                    initializeKey = (Key) mList.getLastKey();
                }

                do {
                    if (mDataSource != null) {
                        mDataSource.removeInvalidatedCallback(mCallback);
                    }
                    mDataSource = dataSourceFactory.create();
                    mDataSource.addInvalidatedCallback(mCallback);

                    mList = new PagedList.Builder<>(mDataSource, config)
                            .setNotifyExecutor(notifyExecutor)
                            .setFetchExecutor(fetchExecutor)
                            .setBoundaryCallback(boundaryCallback)
                            .setInitialKey(initializeKey)
                            .build();
                } while (mList.isDetached());
                return mList;
            }
        }.getLiveData();
    }
复制代码
```

new 了一个ComputableLiveData，最后调用其getLiveData()返回最终的LiveData对象。

```
public abstract class ComputableLiveData<T> {
    @SuppressWarnings("WeakerAccess") /* synthetic access */
    final Executor mExecutor;
    @SuppressWarnings("WeakerAccess") /* synthetic access */
    final LiveData<T> mLiveData;
    @SuppressWarnings("WeakerAccess") /* synthetic access */
    final AtomicBoolean mInvalid = new AtomicBoolean(true);
    @SuppressWarnings("WeakerAccess") /* synthetic access */
    final AtomicBoolean mComputing = new AtomicBoolean(false);
    @SuppressWarnings("WeakerAccess")
    public ComputableLiveData() {
        this(ArchTaskExecutor.getIOThreadExecutor());
    }

    @SuppressWarnings("WeakerAccess")
    public ComputableLiveData(@NonNull Executor executor) {
        mExecutor = executor;
        mLiveData = new LiveData<T>() {
            @Override
            protected void onActive() {
                mExecutor.execute(mRefreshRunnable);
            }
        };
    }

    @NonNull
    public LiveData<T> getLiveData() {
        return mLiveData;
    }

    @VisibleForTesting
    final Runnable mRefreshRunnable = new Runnable() {
        @WorkerThread
        @Override
        public void run() {
            boolean computed;
            do {
                computed = false;
                // 计算智能在一个线程池中进行
                if (mComputing.compareAndSet(false, true)) {
                    // as long as it is invalid, keep computing.
                    try {
                        T value = null;
                        while (mInvalid.compareAndSet(true, false)) {
                            computed = true;
                            value = compute();
                        }
                        if (computed) {
                            mLiveData.postValue(value);
                        }
                    } finally {
                        // release compute lock
                        mComputing.set(false);
                    }
                }
            } while (computed && mInvalid.get());
        }
    };

    @VisibleForTesting
    final Runnable mInvalidationRunnable = new Runnable() {
        @MainThread
        @Override
        public void run() {
            boolean isActive = mLiveData.hasActiveObservers();
            if (mInvalid.compareAndSet(false, true)) {
                if (isActive) {
                    mExecutor.execute(mRefreshRunnable);
                }
            }
        }
    };
    //该方法会重新触发compute()方法
    public void invalidate() {
        ArchTaskExecutor.getInstance().executeOnMainThread(mInvalidationRunnable);
    }

    @WorkerThread
    protected abstract T compute();
}
复制代码
```

可以看到这个ComputableLiveData类并不是一个真正的LiveData，而是包装了一个LiveData，在其构造方法中初始了成员变量mLiveData，并重写了其onActive()方法。

这个地方需要对LiveData有一点了解，可以看之前的文章[Android Jetpack之LiveData](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fmingyunxiaohai%2Farticle%2Fdetails%2F89429932)。当LiveData对象处于活动状态的时候会回调onActive()方法。LiveData是生命周期感知组件，它处于活跃状态，说明当前Activity或者Fragment处于活跃状态，也就是肯定在onStart()生命周期后面。所以这里会立马执行onActive()方法。

手动调用invalidate()方法，可以手动触发onActive方法中的逻辑。

onActive()方法中调用了`mExecutor.execute(mRefreshRunnable);`去线程池中执行mRefreshRunnable这个Runnable对象，run方法中执行了compute()这个抽象方法，这个方法的唯一实现是在前面new ComputableLiveData的时候实现的。在复制过来看一下

```
   protected PagedList<Value> compute() {
                @Nullable Key initializeKey = initialLoadKey;
                if (mList != null) {
                    initializeKey = (Key) mList.getLastKey();
                }

                do {
                    if (mDataSource != null) {
                        mDataSource.removeInvalidatedCallback(mCallback);
                    }
                 //拿到我们传入的DataSource
                mDataSource = dataSourceFactory.create();
                //给DataSource注册一个回调，用来监听DataSource被置为无效事件 
                mDataSource.addInvalidatedCallback(mCallback);

                mList = new PagedList.Builder<>(mDataSource, config)
                            .setNotifyExecutor(notifyExecutor)
                            .setFetchExecutor(fetchExecutor)
                            .setBoundaryCallback(boundaryCallback)
                            .setInitialKey(initializeKey)
                            .build();
                } while (mList.isDetached());
                return mList;
            }
复制代码
```

拿到我们传入的DataSource，然后给它添加一个回调用来监听DataSource被置为无效事件。坚挺到无效事件之后会重新触发compute()方法。最后通过PagedList.Builder构建一个PagedList对象。

跟进它的build方法

```
    public PagedList<Value> build() {
            if (mNotifyExecutor == null) {
                throw new IllegalArgumentException("MainThreadExecutor required");
            }
            if (mFetchExecutor == null) {
                throw new IllegalArgumentException("BackgroundThreadExecutor required");
            }
            return PagedList.create(mDataSource,mNotifyExecutor,mFetchExecutor, mBoundaryCallback,mConfig,mInitialKey);
        }
复制代码
```

先判断传入的线程池不能为null，然后PagedList.create方法创建

```
    static <K, T> PagedList<T> create(@NonNull DataSource<K, T> dataSource,
            @NonNull Executor notifyExecutor,
            @NonNull Executor fetchExecutor,
            @Nullable BoundaryCallback<T> boundaryCallback,
            @NonNull Config config,
            @Nullable K key) {
        if (dataSource.isContiguous() || !config.enablePlaceholders) {
            int lastLoad = ContiguousPagedList.LAST_LOAD_UNSPECIFIED;
            if (!dataSource.isContiguous()) {
                //noinspection unchecked
                dataSource = (DataSource<K, T>) ((PositionalDataSource<T>) dataSource)
                        .wrapAsContiguousWithoutPlaceholders();
                if (key != null) {
                    lastLoad = (Integer) key;
                }
            }
            ContiguousDataSource<K, T> contigDataSource = (ContiguousDataSource<K, T>) dataSource;
            return new ContiguousPagedList<>(contigDataSource,
                    notifyExecutor,
                    fetchExecutor,
                    boundaryCallback,
                    config,
                    key,
                    lastLoad);
        } else {
            return new TiledPagedList<>((PositionalDataSource<T>) dataSource,
                    notifyExecutor,
                    fetchExecutor,
                    boundaryCallback,
                    config,
                    (key != null) ? (Integer) key : 0);
        }
    }
复制代码
```

这段代码就是判断该创建哪种DataSource的PagedList。前面我们知道系统默认有三种DataSource分别是PageKeyedDataSource、ItemKeyedDataSource、PositionalDataSource。

`dataSource.isContiguous()`这个判断PageKeyedDataSource、ItemKeyedDataSource返回的是true，PositionalDataSource返回的是false，前面demo中使用的是PageKeyedDataSource所以这里返回true，最终进入到if判断中创建了一个ContiguousPagedList对象。

```
ContiguousPagedList(
            @NonNull ContiguousDataSource<K, V> dataSource,
            @NonNull Executor mainThreadExecutor,
            @NonNull Executor backgroundThreadExecutor,
            @Nullable BoundaryCallback<V> boundaryCallback,
            @NonNull Config config,
            final @Nullable K key,
            int lastLoad) {
        super(new PagedStorage<V>(), mainThreadExecutor, backgroundThreadExecutor,
                boundaryCallback, config);
        mDataSource = dataSource;
        mLastLoad = lastLoad;

        if (mDataSource.isInvalid()) {
            detach();
        } else {
            mDataSource.dispatchLoadInitial(key,
                    mConfig.initialLoadSizeHint,
                    mConfig.pageSize,
                    mConfig.enablePlaceholders,
                    mMainThreadExecutor,
                    mReceiver);
        }
        mShouldTrim = mDataSource.supportsPageDropping()
                && mConfig.maxSize != Config.MAX_SIZE_UNBOUNDED;
    }
复制代码
```

如果mDataSource是无效的就执行`detach()`方法，detach方法注释上说了，该方法尝试把DataSource从PagedList中分离，并且尝试不在加载数据。当数据无法加载的时候mDataSource.isInvalid()就会被置为true，也就不会再继续帮我们分页了

网络数据加载异常是经常发生的事情，那怎么解决呢，有一种办法就是从当前adapter中拿出所有的PagedList数据和配置，然后手动触发请求网络的代码，拿到数据之后，将连个数据合并，并创建一个新的PagedList，最后调用submitList方法。

如果DataSource是有效的就执行`mDataSource.dispatchLoadInitial`方法来分发原始加载，然后就来到PageKeyedDataSource类中的dispatchLoadInitial方法，记住最后一个参数mReceiver，加载完数据后就使用它将数据回调回来。

```
  @Override
    final void dispatchLoadInitial(@Nullable Key key, int initialLoadSize, int pageSize,
            boolean enablePlaceholders, @NonNull Executor mainThreadExecutor,
            @NonNull PageResult.Receiver<Value> receiver) {
        LoadInitialCallbackImpl<Key, Value> callback =
                new LoadInitialCallbackImpl<>(this, enablePlaceholders, receiver);
        loadInitial(new LoadInitialParams<Key>(initialLoadSize, enablePlaceholders), callback);

        callback.mCallbackHelper.setPostExecutor(mainThreadExecutor);
    }
复制代码
```

创建了一个LoadInitialCallbackImpl对象然后调用loadInitial方法来加载初始化的数据

这里loadInitial是一个抽象方法，最终就调用了我们最开始第一步中自定义的ArticleDataSource中的loadInitial开始加载原始数据

数据加载完成之后，通过`initialCallback.onResult`方法将数据回调，也就是到了LoadInitialCallbackImpl类中的onResult方法

```
    static class LoadInitialCallbackImpl<Key, Value> extends LoadInitialCallback<Key, Value> {
        final LoadCallbackHelper<Value> mCallbackHelper;
        private final PageKeyedDataSource<Key, Value> mDataSource;
        private final boolean mCountingEnabled;
        LoadInitialCallbackImpl(@NonNull PageKeyedDataSource<Key, Value> dataSource,
                boolean countingEnabled, @NonNull PageResult.Receiver<Value> receiver) {
            mCallbackHelper = new LoadCallbackHelper<>(
                    dataSource, PageResult.INIT, null, receiver);
            mDataSource = dataSource;
            mCountingEnabled = countingEnabled;
        }

        @Override
        public void onResult(@NonNull List<Value> data, int position, int totalCount,
                @Nullable Key previousPageKey, @Nullable Key nextPageKey) {
            if (!mCallbackHelper.dispatchInvalidResultIfInvalid()) {
                LoadCallbackHelper.validateInitialLoadParams(data, position, totalCount);

                // setup keys before dispatching data, so guaranteed to be ready
                mDataSource.initKeys(previousPageKey, nextPageKey);

                int trailingUnloadedCount = totalCount - position - data.size();
                if (mCountingEnabled) {
                    mCallbackHelper.dispatchResultToReceiver(new PageResult<>(
                            data, position, trailingUnloadedCount, 0));
                } else {
                    mCallbackHelper.dispatchResultToReceiver(new PageResult<>(data, position));
                }
            }
        }

        @Override
        public void onResult(@NonNull List<Value> data, @Nullable Key previousPageKey,
                @Nullable Key nextPageKey) {
            if (!mCallbackHelper.dispatchInvalidResultIfInvalid()) {
                mDataSource.initKeys(previousPageKey, nextPageKey);
                mCallbackHelper.dispatchResultToReceiver(new PageResult<>(data, 0, 0, 0));
            }
        }
    }
复制代码
```

onResult方法还有个重载的方法，还有各种判断，不过最终都会走到`mCallbackHelper.dispatchResultToReceiver`方法，分发结果。

```
  void dispatchResultToReceiver(final @NonNull PageResult<T> result) {
            Executor executor;
            //一个callback只能调用一次
            synchronized (mSignalLock) {
                if (mHasSignalled) {
                    throw new IllegalStateException(
                            "callback.onResult already called, cannot call again.");
                }
                mHasSignalled = true;
                executor = mPostExecutor;
            }

            if (executor != null) {
                executor.execute(new Runnable() {
                    @Override
                    public void run() {
                        mReceiver.onPageResult(mResultType, result);
                    }
                });
            } else {
                mReceiver.onPageResult(mResultType, result);
            }
        }
复制代码
```

这里可以看到最终通过mReceiver的onPageResult方法把数据回调会去，mReceiver就是前面调用dispatchLoadInitial方法的时候传过来的。

onPageResult方法中把回调回来的数据保存在**PagedStorage**中的成员变量`private final ArrayList<List<T>> mPages;`中完毕。

#### 2.2 如何触发分页加载的

分页加载的逻辑就应该在跟RecyclerView有关的adapter中触发了，在adapter的onBindViewHolder方法中，我们使用`getItem(position)`方法获取当前item的对象。这个方法时PagedListAdapter提供的。

```
   protected T getItem(int position) {
        return mDiffer.getItem(position);
    }
复制代码
```

mDiffer是AsyncPagedListDiffer的对象，所以调用AsyncPagedListDiffer中的getItem方法

```
  public T getItem(int index) {
        if (mPagedList == null) {
            if (mSnapshot == null) {
                throw new IndexOutOfBoundsException(
                        "Item count is zero, getItem() call is invalid");
            } else {
                return mSnapshot.get(index);
            }
        }
        mPagedList.loadAround(index);
        return mPagedList.get(index);
    }
复制代码
```

如果mPagedList不为null，就调用mPagedList.loadAround方法

```
    public void loadAround(int index) {
        if (index < 0 || index >= size()) {
            throw new IndexOutOfBoundsException("Index: " + index + ", Size: " + size());
        }

        mLastLoad = index + getPositionOffset();
        loadAroundInternal(index);

        mLowestIndexAccessed = Math.min(mLowestIndexAccessed, index);
        mHighestIndexAccessed = Math.max(mHighestIndexAccessed, index);
        
        tryDispatchBoundaryCallbacks(true);
    }
复制代码
```

又调用了loadAroundInternal方法，它是一个抽象方法，最终调用到PagedList的子类ContiguousPagedList中的loadAroundInternal方法

```
 protected void loadAroundInternal(int index) {
        int prependItems = getPrependItemsRequested(mConfig.prefetchDistance, index,
                mStorage.getLeadingNullCount());
        int appendItems = getAppendItemsRequested(mConfig.prefetchDistance, index,
                mStorage.getLeadingNullCount() + mStorage.getStorageCount());

        mPrependItemsRequested = Math.max(prependItems, mPrependItemsRequested);
        //如果向前预加载是数量大于0
        if (mPrependItemsRequested > 0) {
            schedulePrepend();
        }
        mAppendItemsRequested = Math.max(appendItems, mAppendItemsRequested);
        //如果向后预加载的数量大于0
        if (mAppendItemsRequested > 0) {
            scheduleAppend();
        }
    }
复制代码
```

如果向前预加载是数量大于0，就调用`schedulePrepend();`方法，该方法最终会调用到DataSource中的loadBefore方法。

```
 private void schedulePrepend() {
        ......
        mBackgroundThreadExecutor.execute(new Runnable() {
            @Override
            public void run() {
                if (isDetached()) {
                    return;
                }
                if (mDataSource.isInvalid()) {
                    detach();
                } else {
                    mDataSource.dispatchLoadBefore(position, item, mConfig.pageSize,
                            mMainThreadExecutor, mReceiver);
                }
            }
        });
    }
     @Override
    final void dispatchLoadBefore(int currentBeginIndex, @NonNull Value currentBeginItem,int pageSize, @NonNull Executor mainThreadExecutor,@NonNull PageResult.Receiver<Value> receiver) {
        @Nullable Key key = getPreviousKey();
        if (key != null) {
            loadBefore(new LoadParams<>(key, pageSize),
                    new LoadCallbackImpl<>(this, PageResult.PREPEND, mainThreadExecutor, receiver));
        } else {
            receiver.onPageResult(PageResult.PREPEND, PageResult.<Value>getEmptyResult());
        }
    }
复制代码
```

如果是向后预加载会调用`scheduleAppend();`方法，该方法最终会调用到DataSource中的loadAfter方法，最终触发分页加载

```
   private void scheduleAppend() {
       ......
        mBackgroundThreadExecutor.execute(new Runnable() {
            @Override
            public void run() {
                if (isDetached()) {
                    return;
                }
                if (mDataSource.isInvalid()) {
                    detach();
                } else {
                    mDataSource.dispatchLoadAfter(position, item, mConfig.pageSize,
                            mMainThreadExecutor, mReceiver);
                }
            }
        });
    }
    final void dispatchLoadAfter(int currentEndIndex, @NonNull Value currentEndItem,int pageSize, @NonNull Executor mainThreadExecutor,@NonNull PageResult.Receiver<Value> receiver) {
        @Nullable Key key = getNextKey();
        if (key != null) {
            loadAfter(new LoadParams<>(key, pageSize),
                    new LoadCallbackImpl<>(this, PageResult.APPEND, mainThreadExecutor, receiver));
        } else {
            receiver.onPageResult(PageResult.APPEND, PageResult.<Value>getEmptyResult());
        }
    }
复制代码
```

调用loadAfter的时候传入了一个回调的实现类LoadCallbackImpl，当loadAfter中的数据加载完成之后，会通过这个类把结果回调回来

```
   static class LoadCallbackImpl<Key, Value> extends LoadCallback<Key, Value> {
        final LoadCallbackHelper<Value> mCallbackHelper;
        private final PageKeyedDataSource<Key, Value> mDataSource;
        LoadCallbackImpl(@NonNull PageKeyedDataSource<Key, Value> dataSource,
                @PageResult.ResultType int type, @Nullable Executor mainThreadExecutor,
                @NonNull PageResult.Receiver<Value> receiver) {
            mCallbackHelper = new LoadCallbackHelper<>(
                    dataSource, type, mainThreadExecutor, receiver);
            mDataSource = dataSource;
        }

        @Override
        public void onResult(@NonNull List<Value> data, @Nullable Key adjacentPageKey) {
            if (!mCallbackHelper.dispatchInvalidResultIfInvalid()) {
                if (mCallbackHelper.mResultType == PageResult.APPEND) {
                    mDataSource.setNextKey(adjacentPageKey);
                } else {
                    mDataSource.setPreviousKey(adjacentPageKey);
                }
                mCallbackHelper.dispatchResultToReceiver(new PageResult<>(data, 0, 0, 0));
            }
        }
    }
复制代码
```

最终会跟初始化数据一样，调用`mCallbackHelper.dispatchResultToReceiver`方法，将数据保存在PagedList的成员变量PagedStorage中。

到这里初始化的流程和分页加载的流程就都了解了，本文是以PageKeyedDataSource为例往下跟的，其他两个ItemKeyedDataSource、PositionalDataSource原理基本一样。

#### 2.3 如何刷新RecyclerView

这个要从Activity中`mAdapter.submitList(datasBeans);`开始看了。

```
 public void submitList(@Nullable PagedList<T> pagedList) {
        mDiffer.submitList(pagedList);
    }
复制代码
```

也是调用了AsyncPagedListDiffer中的submitList。其实可以看到PagedListAdapter其实只是继承了RecyclerView.Adapter，它内部跟Paging有关的操作都交给了AsyncPagedListDiffer这个类取做了。

```
 public void submitList(@Nullable final PagedList<T> pagedList) {
        submitList(pagedList, null);
    }
 public void submitList(@Nullable final PagedList<T> pagedList,
         @Nullable final Runnable commitCallback) {
          
          ......
          
            if (mPagedList == null && mSnapshot == null) {
            // fast simple first insert
            mPagedList = pagedList;
            pagedList.addWeakCallback(null, mPagedListCallback);

            // dispatch update callback after updating mPagedList/mSnapshot
            mUpdateCallback.onInserted(0, pagedList.size());

            onCurrentListChanged(null, pagedList, commitCallback);
            return;
        }
          if (mPagedList != null) {
            mPagedList.removeWeakCallback(mPagedListCallback);
            mSnapshot = (PagedList<T>) mPagedList.snapshot();
            mPagedList = null;
        }

        if (mSnapshot == null || mPagedList != null) {
            throw new IllegalStateException("must be in snapshot state to diff");
        }

        final PagedList<T> oldSnapshot = mSnapshot;
        final PagedList<T> newSnapshot = (PagedList<T>) pagedList.snapshot();
        mConfig.getBackgroundThreadExecutor().execute(new Runnable() {
            @Override
            public void run() {
                final DiffUtil.DiffResult result;
                result = PagedStorageDiffHelper.computeDiff(
                        oldSnapshot.mStorage,
                        newSnapshot.mStorage,
                        mConfig.getDiffCallback());
                 mMainThreadExecutor.execute(new Runnable() {
                    @Override
                    public void run() {
                        if (mMaxScheduledGeneration == runGeneration) {
                            latchPagedList(pagedList, newSnapshot, result,
                                    oldSnapshot.mLastLoad, commitCallback);
                        }
                    }
                });
              
            }
        });
    }
复制代码
```

**如果mPagedList为null或者他的快照为null**，说明这是第一次初始化数据，调用`mUpdateCallback.onInserted(0, pagedList.size());`方法将数据插入到列表中。

这个mUpdateCallback是在AsyncPagedListDiffer构造方法中初始化的，是AdapterListUpdateCallback类型。AdapterListUpdateCallback是RecyclerView包中的类，内部调用了RecyclerView.Adapter的各种notify的方法来刷新界面。

```
public final class AdapterListUpdateCallback implements ListUpdateCallback {
    @NonNull
    private final RecyclerView.Adapter mAdapter;

    public AdapterListUpdateCallback(@NonNull RecyclerView.Adapter adapter) {
        mAdapter = adapter;
    }

    @Override
    public void onInserted(int position, int count) {
        mAdapter.notifyItemRangeInserted(position, count);
    }

    @Override
    public void onRemoved(int position, int count) {
        mAdapter.notifyItemRangeRemoved(position, count);
    }

    @Override
    public void onMoved(int fromPosition, int toPosition) {
        mAdapter.notifyItemMoved(fromPosition, toPosition);
    }

    @Override
    public void onChanged(int position, int count, Object payload) {
        mAdapter.notifyItemRangeChanged(position, count, payload);
    }
}
复制代码
```

**如果mPagedList不为null**，说明是分页加载数据，调用`PagedStorageDiffHelper.computeDiff`方法计算出差分结果后，切换到主线程调用`latchPagedList`方法刷新

```
void latchPagedList(
            @NonNull PagedList<T> newList,
            @NonNull PagedList<T> diffSnapshot,
            @NonNull DiffUtil.DiffResult diffResult,
            int lastAccessIndex,
            @Nullable Runnable commitCallback) {
               
               ......
                PagedStorageDiffHelper.dispatchDiff(mUpdateCallback,
                previousSnapshot.mStorage, newList.mStorage, diffResult);
                
               ......
            }
复制代码
```

最后也是通过mUpdateCallback的各种回调来最后更新列表的。

### 总结

总结一下Paging框架的工作流程：

初始化数据：

- `LivePagedListBuilder#build()`->`LivePagedListBuilder#create`创建一个ComputableLiveData最后返回一个LiveData数据观察者
- LiveData的`onActive()`方法会根据activity/fragment的生命周期自动触发，加载的逻辑也在这开始
- `ComputableLiveData#compute`-`PagedList#build`->`PagedList#create`。根据我们传入的不同的DataSource返回不同的PagedList。ContiguousPagedList或者TiledPagedList
- 在创建出来的ContiguousPagedList或者TiledPagedList的构造方法中进行初始数据的分发和加载
- 数据加载完成之后，通过`LoadInitialCallbackImpl#onResult`回调回来最终保存在PagedList的成员变量PagedStorage中

分页加载数据：

- `PagedListAdapter#getItem`->`AsyncPagedListDiffer#getItem`->`PagedList#loadAround`-`>PagedList#loadAroundInternal`。loadAroundInternal是抽象方法
- 在子类ContiguousPagedList或者TiledPagedList的loadAroundInternal方法中计算出需要向前加载还是向后加载，并触发相关方法
- 数据加载完成之后，通过`LoadCallbackImpl#onResult`方法回调结果，最终保存在PagedList的成员变量PagedStorage中

刷新列表

- `mAdapter#submitList;`->`AsyncPagedListDiffer#submitList`
- `AdapterListUpdateCallback#onInserted`，`AdapterListUpdateCallback#onRemoved`
