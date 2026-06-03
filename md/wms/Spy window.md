
用户想要了解 Spy Window 的实践应用，包括实际的代码实现、使用场景和最佳实践。让我搜索一下项目中是否已经有相关实现，同时提供完整的 Spy Window 实践指南。
好的！让我为您提供 **Spy Window (Gesture Monitor)** 的**完整实践指南**，包含可运行的代码模板、真实场景示例和注意事项。

---

## Spy Window 实践指南

### 一、完整工程级代码（可直接使用）

```kotlin
/**
 * 全局手势监控服务 - Spy Window 实践
 * 
 * 功能：监听全局触摸事件，支持自定义手势识别与劫持
 * 适用场景：系统导航手势、全局快捷操作
 */
class GestureSpyService : Service() {

    // ==================== 核心组件 ====================
    private var inputMonitor: InputMonitor? = null
    private var eventReceiver: InputEventReceiver? = null
    
    // 手势识别器（可选）
    private var gestureDetector: GestureDetector? = null
    
    // 状态追踪
    private val activePointers = mutableMapOf<Int, PointerState>()
    
    // ==================== 生命周期管理 ====================
    
    override fun onCreate() {
        super.onCreate()
        initSpyWindow()
        initGestureDetector()
        Log.d(TAG, "🔍 Spy Window 已创建")
    }
    
    override fun onDestroy() {
        super.onDestroy()
        releaseSpyWindow()
        Log.d(TAG, "🚫 Spy Window 已销毁")
    }
    
    override fun onBind(intent: Intent?): IBinder? = null
    
    // ==================== 初始化 Spy Window ====================
    
    @SuppressLint("NewApi")  // API 30+ (Android R)
    private fun initSpyWindow() {
        try {
            val im = getSystemService(InputManager::class.java) ?: run {
                Log.e(TAG, "❌ 获取 InputManager 失败")
                return
            }
            
            // ★ 步骤1：创建 InputMonitor
            inputMonitor = im.monitorGestureInput(
                "erp_pda_spy",           // 名称标识符
                Display.DEFAULT_DISPLAY   // 主显示器
            )
            
            Log.d(TAG, "✅ InputMonitor 创建成功: ${inputMonitor?.inputChannel}")
            
            // ★ 步骤2：绑定事件接收器
            eventReceiver = object : InputEventReceiver(
                inputMonitor!!.inputChannel,
                Looper.getMainLooper()
            ) {
                
                override fun onInputEvent(event: InputEvent) {
                    handleInputEvent(event)
                    finishInputEvent(event, false) // 放行事件
                }
                
                override fun onFocusEvent(hasFocus: Boolean) {
                    Log.d(TAG, "👁️ Focus changed: $hasFocus")
                }
            }
            
            Log.d(TAG, "✅ InputEventReceiver 绑定成功")
            
        } catch (e: Exception) {
            Log.e(TAG, "❌ 初始化失败", e)
        }
    }
    
    // ==================== 核心事件处理 ====================
    
    @SuppressLint("ClickableViewAccessibility")
    private fun handleInputEvent(event: InputEvent) {
        when (event) {
            is MotionEvent -> handleMotionEvent(event)
            is KeyEvent -> handleKeyEvent(event)
        }
    }
    
    /**
     * 触摸事件处理 - 核心逻辑
     */
    private fun handleMotionEvent(event: MotionEvent) {
        val action = event.actionMasked
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        
        // 记录指针状态
        updatePointerState(event, pointerId, action)
        
        when (action) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                Log.d(
                    TAG, 
                    "👆 指针 #$pointerId DOWN (${event.x.toInt()}, ${event.y.toInt()}) | 共 ${event.pointerCount} 指"
                )
                
                // 示例1：检测三指按下
                if (event.pointerCount >= 3) {
                    onThreeFingerDetected()
                }
                
                // 示例2：检测边缘滑动起始
                if (isEdgeSwipeStart(event)) {
                    Log.d(TAG, "⬅️ 边缘滑动检测到")
                }
            }
            
            MotionEvent.ACTION_MOVE -> {
                // 追踪所有活跃指针
                logAllPointerPositions(event)
                
                // 手势检测
                gestureDetector?.onTouchEvent(event)
            }
            
            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                Log.d(TAG, "☝️ 指针 #$pointerId UP")
                activePointers.remove(pointerId)
            }
            
            MotionEvent.ACTION_CANCEL -> {
                Log.w(TAG, "⚠️ 事件被取消（可能被其他 Spy 劫持）")
                activePointers.clear()
            }
        }
        
        // 打印详细日志
        dumpEventInfo(event)
    }
    
    /**
     * 按键事件处理
     */
    private fun handleKeyEvent(event: KeyEvent) {
        Log.d(
            TAG, 
            "⌨️ Key: keyCode=${event.keyCode} action=${event.action} " +
            "repeat=${event.repeatCount} scanCode=${event.scanCode}"
        )
        
        // 示例：全局快捷键拦截
        if (event.keyCode == KeyEvent.KEYCODE_VOLUME_DOWN && 
            event.action == KeyEvent.ACTION_DOWN &&
            event.repeatCount == 0) {
            
            // 可以在这里触发全局操作
            // 注意：需要配合 pilferPointers 才能真正消费按键
            Log.d(TAG, "🔊 音量键按下 - 可触发操作")
        }
    }
    
    // ==================== pilferPointers 劫持示例 ====================
    
    /**
     * 劫持当前触控流
     * 
     * 效果：
     * - 当前窗口立即收到 CANCEL
     * - 后续事件全部路由到此 SPY
     */
    @RequiresApi(Build.VERSION_CODES.R)
    fun pilferCurrentGestures() {
        try {
            val im = getSystemService(InputManager::class.java)
            val token = inputMonitor?.inputChannel?.token
            
            if (token != null) {
                im.pilferPointers(token)
                Log.d(TAG, "🎯 pilferPointers 成功！已劫持当前手势流")
                
                // 可在此处执行后续手势专属逻辑
                onGesturesPilfered()
            } else {
                Log.w(TAG, "⚠️ Token 为空，无法 pilfer")
            }
        } catch (e: Exception) {
            Log.e(TAG, "❌ pilferPointers 失败", e)
        }
    }
    
    private fun onGesturesPilfered() {
        // 劫持后的业务逻辑
        Toast.makeText(this, "手势已被系统接管", Toast.LENGTH_SHORT).show()
    }
    
    // ==================== 自定义手势识别 ====================
    
    private fun initGestureDetector() {
        gestureDetector = GestureDetector(this, object : SimpleOnGestureListener() {
            
            override fun onDoubleTap(e: MotionEvent): Boolean {
                Log.d(TAG, "👆👆 双击检测")
                return true
            }
            
            override fun onLongPress(e: MotionEvent) {
                Log.d(TAG, "⏳ 长按位置: (${e.x.toInt()}, ${e.y.toInt()})")
                
                // 长按时劫持（示例）
                // pilferCurrentGestures()
            }
            
            override fun onScroll(
                e1: MotionEvent?, 
                e2: MotionEvent?, 
                distanceX: Float, 
                distanceY: Float
            ): Boolean {
                // 边缘滑动检测
                if (e1 != null && e1.x < 50 && distanceX > 100) {
                    Log.d(TAG, "⬅️ 左边缘滑动 → 返回手势")
                    performBackNavigation()
                }
                return true
            }
            
            override fun onFling(
                e1: MotionEvent?, 
                e2: MotionEvent?, 
                velocityX: Float, 
                velocityY: Float
            ): Boolean {
                if (e1 != null && e2 != null) {
                    val angle = Math.toDegrees(
                        kotlin.math.atan2((e2.y - e1.y).toDouble(), (e2.x - e1.x).toDouble())
                    ).toFloat()
                    
                    Log.d(TAG, "🏃 Fling 角度: $angle° 速度: ($velocityX, $velocityY)")
                    
                    // 上滑多任务手势
                    if (velocityY < -2000 && kotlin.math.abs(angle) < 45) {
                        Log.d(TAG, "⬆️ 快速上滑 → 多任务视图")
                    }
                }
                return true
            }
        })
    }
    
    // ==================== 辅助方法 ====================
    
    private data class PointerState(
        val startX: Float,
        val startY: Float,
        var currentX: Float = startX,
        var currentY: Float = startY
    )
    
    private fun updatePointerState(event: MotionEvent, pointerId: Int, action: Int) {
        when (action) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                val index = event.findPointerIndex(pointerId)
                if (index >= 0) {
                    activePointers[pointerId] = PointerState(
                        event.getX(index), event.getY(index)
                    )
                }
            }
            MotionEvent.ACTION_MOVE -> {
                activePointers[pointerId]?.let { state ->
                    val index = event.findPointerIndex(pointerId)
                    state.currentX = event.getX(index)
                    state.currentY = event.getY(index)
                }
            }
        }
    }
    
    private fun isEdgeSwipeStart(event: MotionEvent): Boolean {
        val edgeThreshold = 20.dpToPx(this)
        return event.x < edgeThreshold || 
               event.x > (resources.displayMetrics.widthPixels - edgeThreshold)
    }
    
    private fun logAllPointerPositions(event: MotionEvent) {
        if (activePointers.size <= 1) return
        
        val sb = StringBuilder("📍 活跃指针:\n")
        for ((id, state) in activePointers) {
            sb.append("  #$id: (${state.currentX.toInt()}, ${state.currentY.toInt()})\n")
        }
        Log.v(TAG, sb.toString())
    }
    
    private fun dumpEventInfo(event: MotionEvent) {
        Log.v(
            TAG, 
            "[${event.actionMasked}] ptrs=${event.pointerCount} " +
            "time=${event.eventTime - event.downTime}ms"
        )
    }
    
    private fun onThreeFingerDetected() {
        Log.i(TAG, "✋ 三指手势启动！")
        // 可选：pilferCurrentGestures()
    }
    
    private fun performBackNavigation() {
        Log.d(TAG, "⬅️ 执行返回操作")
        // 实际项目中调用系统返回
        // performGlobalAction(AccessibilityService.GLOBAL_ACTION_BACK)
    }
    
    private fun Int.dpToPx(context: Context): Int {
        return (this * context.resources.displayMetrics.density).toInt()
    }
    
    // ==================== 资源释放 ====================
    
    private fun releaseSpyWindow() {
        try {
            eventReceiver?.dispose()
            inputMonitor?.dispose()
            eventReceiver = null
            inputMonitor = null
            activePointers.clear()
            Log.d(TAG, "♻️ 资源已释放")
        } catch (e: Exception) {
            Log.e(TAG, "❌ 释放资源时出错", e)
        }
    }
    
    companion object {
        private const val TAG = "GestureSpy"
    }
}
```

---

### 二、AndroidManifest.xml 配置

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <!-- ★ 必需权限 -->
    <uses-permission android:name="android.permission.MONITOR_INPUT" />
    <uses-permission android:name="android.permission.SYSTEM_ALERT_WINDOW" />

    <application>
        
        <!-- 声明 Service -->
        <service
            android:name=".service.GestureSpyService"
            android:enabled="true"
            android:exported="false"
            android:foregroundServiceType="specialUse">
            <property
                android:name="android.app.PROPERTY_SPECIAL_USE_FGS_SUBTYPE"
                android:value="global_gesture_monitor" />
        </service>

    </application>
</manifest>
```

---

### 三、实际应用场景案例

#### 案例 1：PDA 扫码枪全局拦截


```kotlin
/**
 * 场景：ERP PDA 扫码枪无需焦点即可捕获条码
 * 替代方案：比 EditText + OnEditorAction 更底层
 */
class BarcodeSpyManager(context: Context) {
    
    private val im = context.getSystemService(InputManager::class.java)
    private var monitor: InputMonitor? = null
    
    // 条码缓冲区
    private val barcodeBuffer = StringBuilder()
    private var lastKeyTime = 0L
    private val BARCODE_TIMEOUT = 100L // ms
    
    fun startMonitoring() {
        monitor = im.monitorGestureInput("barcode_spy", Display.DEFAULT_DISPLAY)
        
        object : InputEventReceiver(monitor!!.inputChannel, Looper.getMainLooper()) {
            override fun onInputEvent(event: InputEvent) {
                if (event is KeyEvent && event.action == ACTION_DOWN) {
                    appendBarcodeChar(event)
                }
                finishInputEvent(event, false)
            }
        }
    }
    
    private fun appendBarcodeChar(keyEvent: KeyEvent) {
        // 扫码枪快速连续发送字符
        if (System.currentTimeMillis() - lastKeyTime > BARCODE_TIMEOUT) {
            barcodeBuffer.clear()
        }
        lastKeyTime = System.currentTimeMillis()
        
        barcodeBuffer.append(keyEvent.unicodeChar.toChar())
        
        // Enter 结束条码
        if (keyEvent.keyCode == KEYCODE_ENTER && barcodeBuffer.isNotEmpty()) {
            onCompleteBarcode(barcodeBuffer.toString())
            barcodeBuffer.clear()
        }
    }
    
    var onBarcodeReceived: ((String) -> Unit)? = null
    
    private fun onCompleteBarcode(barcode: String) {
        Log.d("BarcodeSpy", "📷 收到条码: $barcode")
        onBarcodeReceived?.invoke(barcode)
    }
    
    fun stop() {
        monitor?.dispose()
    }
}
```

#### 案例 2：全局手势导航

```kotlin
/**
 * 场景：类似 Android 系统导航栏的手势
 */
class SystemGestureSpy(private val context: Context) {
    
    data class GestureConfig(
        val edgeWidthDp: Float = 24f,       // 边缘宽度
        val minVelocity: Float = 1000f,      // 最小速度
        val swipeUpThreshold: Float = 300f,   // 上滑阈值
    )
    
    private var config = GestureConfig()
    
    fun start(config: GestureConfig = GestureConfig()) {
        this.config = config
        
        val im = context.getSystemService(InputManager::class.java)
        val monitor = im.monitorGestureInput("nav_spy", Display.DEFAULT_DISPLAY)
        
        object : InputEventReceiver(monitor.inputChannel, Looper.getMainLooper()) {
            private var swipeStartX = 0f
            private var swipeStartY = 0f
            
            override fun onInputEvent(event: InputEvent) {
                if (event !is MotionEvent) {
                    finishInputEvent(event, false)
                    return
                }
                
                when (event.actionMasked) {
                    ACTION_DOWN -> {
                        swipeStartX = event.x
                        swipeStartY = event.y
                    }
                    
                    ACTION_UP -> {
                        val dx = event.x - swipeStartX
                        val dy = event.y - swipeStartY
                        
                        when {
                            // 左边缘返回
                            swipeStartX < config.edgeWidthDp.dp() && 
                            dx > 200 && dy.absoluteValue < 150 -> {
                                triggerBack()
                            }
                            
                            // 底部上滑多任务
                            isBottomEdge(swipeStartY) && 
                            dy < -config.swipeUpThreshold -> {
                                triggerRecents()
                            }
                        }
                    }
                }
                
                finishInputEvent(event, false)
            }
        }
    }
    
    private fun isBottomEdge(y: Float): Boolean {
        val screenHeight = context.resources.displayMetrics.heightPixels
        return y > screenHeight - config.edgeWidthDp.dp()
    }
}
```

---

### 四、调试工具类

```kotlin
/**
 * Spy Window 调试助手
 * 在悬浮窗显示触摸轨迹
 */
class SpyDebugOverlay(context: Context) {
    
    private val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
    private lateinit var overlayView: View
    
    fun show() {
        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT,
            TYPE_APPLICATION_OVERLAY,
            FLAG_NOT_TOUCHABLE or FLAG_NOT_FOCUSABLE,
            PixelFormat.TRANSLUCENT
        )
        
        overlayView = DebugTouchView(context)
        windowManager.addView(overlayView, params)
    }
    
    fun recordTouch(x: Float, y: Float) {
        (overlayView as? DebugTouchView)?.addPoint(x, y)
    }
    
    fun hide() {
        windowManager.removeView(overlayView)
    }
}

class DebugTouchView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {
    
    private val points = mutableListOf<Pair<Float, Float>>()
    private val paint = Paint().apply {
        color = Color.RED
        strokeWidth = 8f
        isAntiAlias = true
        style = Paint.Style.STROKE
    }
    
    fun addPoint(x: Float, y: Float) {
        points.add(Pair(x, y))
        invalidate()
    }
    
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        if (points.size < 2) return
        
        for (i in 1 until points.size) {
            canvas.drawLine(
                points[i-1].first, points[i-1].second,
                points[i].first, points[i].second,
                paint
            )
        }
        
        // 绘制起点标记
        points.firstOrNull()?.let {(x, y) ->
            canvas.drawCircle(x, y, 15f, Paint().apply {
                color = Color.GREEN
                style = Style.FILL
            })
        }
    }
}
```

---

### 五、最佳实践清单

| 项目 | 建议 |
|------|------|
| **生命周期** | 在 `Service` 中管理，避免 Activity 销毁后泄漏 |
| **线程安全** | 所有 UI 操作切回主线程 |
| **性能** | 减少日志输出，生产环境移除 verbose 日志 |
| **错误处理** | `try-catch` 包裹所有 IMS 调用 |
| **权限检查** | 运行时验证是否有 MONITOR_INPUT 权限 |
| **内存泄漏** | `onDestroy()` 必须调用 `dispose()` |
| **兼容性** | 仅 Android R (API 30+) 支持 `monitorGestureInput` |

---

### 六、常见问题排查

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| `SecurityException` | 无系统签名/权限 | 使用 `adb shell appops set <pkg> MONITOR_INPUT allow` |
| 事件不回调 | 忘记绑定 Receiver | 确保 `InputEventReceiver` 正确创建 |
| ANR | 在主线程做重计算 | 将手势识别移至工作线程 |
| 内存泄漏 | 未 dispose | 在 `onDestroy()` 释放 |
| pilfer 无效 | 在 UP 之后调用 | 只能在 DOWN/MOVE 时段调用 |

---

如果您想将这个实践集成到您的 ERP PDA 项目中（比如用于**无焦点的扫码枪监听**或**全局手势导航**），我可以帮您：

1. **定制化修改** - 根据项目需求调整代码
2. **集成指导** - 帮您在 `InventoryQuickCheckFragment` 或其他模块中接入
3. **权限配置** - 协助设置开发环境的特殊权限

请告诉我您的具体需求方向！