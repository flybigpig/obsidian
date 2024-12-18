```
// 滑动到目标位置
                    var index = it.indexOf(item)
                    // 获取当前的 LinearLayoutManager
                    val layoutManager = recycler_view.layoutManager as LinearLayoutManager
                    // 计算顶部偏移量，确保新项在顶部
                    var topOffset = (layoutManager.findViewByPosition(index)?.top ?: 0) - layoutManager.paddingTop
                    // 平滑滚动到指定位置，并设置顶部偏移量

                    if (topOffset == 0 && index != 0) {
                        recycler_view.scrollToPosition(index)
                        Handler(Looper.getMainLooper()).postDelayed(
                            Runnable {
                                val layoutManager = recycler_view.layoutManager as LinearLayoutManager
                                // 计算顶部偏移量，确保新项在顶部
                                var topOffset = (layoutManager.findViewByPosition(index)?.top ?: 0) - layoutManager.paddingTop
                                recycler_view.scrollBy(0, topOffset- 8)
                            }, 200
                        )
                    } else {
                        recycler_view.smoothScrollToPosition(index)
                        recycler_view.scrollBy(0, topOffset)
                    }

```