# wordIndex



### list

```
mList = findViewById(R.id.list);
ArrayAdapter adapter = new ArrayAdapter<String>(MainActivity2.this,
        R.layout.simple_expandable_list_item_1, mLetters) {
    @NonNull
    @Override
    public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        DisplayMetrics outMetrics = new DisplayMetrics();
        int heightPixels = mList.getMeasuredHeight();
        int perHeight = heightPixels / (mLetters.length);

        View v = LinearLayout.inflate(getApplicationContext(), R.layout.simple_expandable_list_item_1, null);

        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, perHeight);
        v.setLayoutParams(layoutParams);

        TextView t = v.findViewById(R.id.tv);
        t.setText(mLetters[position]);

        return super.getView(position, v, parent);
    }
};

mList.setAdapter(adapter);

mList.setOnTouchListener(new View.OnTouchListener() {

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        int position = ((ListView) v).pointToPosition((int) event.getX(), (int) event.getY());
        if (position < 0 || position >= 27) {
            return false;
        }
        try {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    if (mOnTouch != null) {
                        mOnTouch.onTouch(mLetters[position]);
                    }
                    break;
                case MotionEvent.ACTION_UP:
                    if (mOnTouch != null) {
                        mOnTouch.cancel();
                    }
                    break;
            }
        } catch (Exception e) {

        } finally {
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    Looper.myLooper();
                    if (mTv.getVisibility() == View.VISIBLE) {
                        mTv.setVisibility(View.GONE);
                    }
                    Looper.loop();
                }
            }, 1000);
        }

        return false;
    }
});
```





> 根据坐标作曲当前list点击序号

```
/**
 * 获取ActionBar 高度
 * 需要注意actionbar 是否存在
 *
 * @param context
 * @return
 */
public static int getActionBarHeight(Activity context) {
    TypedValue tv = new TypedValue();
    if (context.getTheme().resolveAttribute(android.R.attr.actionBarSize, tv, true)) {
        return TypedValue.complexToDimensionPixelSize(tv.data,
                context.getResources().getDisplayMetrics());
    }
    return 0;
}

/**
 * 通过坐标获取index
 *
 * @param height
 * @param size
 * @param event
 * @return
 */
public int getPositionfromXY(int height, int size, MotionEvent event, boolean isActionBar) {
    try {
        Rect rect = new Rect();
        int fingerY = -1;
        getWindow().getDecorView().getWindowVisibleDisplayFrame(rect);
        int statusBarHeight = rect.top;
        Rect mTouchFrame = null;
        if (isActionBar) {
            fingerY = (int) event.getY() - getActionBarHeight(this) - statusBarHeight;
        } else {
            fingerY = (int) event.getY() - -statusBarHeight;
        }
        int singleHeight = height / size;
        return (int) (fingerY / singleHeight);
    } catch (Exception e) {
        Log.d("positionsss", e.toString());
    }
    return -1;
}
```







### 自定义view

```
APaintView ap = findViewById(R.id.ap);
ap.setListener(mOnTouch);
```



```
package com.example.myapplicationndk.indexlist;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

public class APaintView extends View {


    private String[] mLetters = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R"
            , "S", "T", "U", "V", "W", "X", "Y", "Z", "#"};
    private int mViewHeight;
    private String mCurrentTouchLetter;
    private onTouch mOnTouch;


    public APaintView(Context context, onTouch listener) {
        super(context);
        mOnTouch = listener;
    }

    public APaintView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mViewHeight = getMeasuredHeight();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);


        // 每个字母所占用的高度
        float singleHeight = (float) mViewHeight / mLetters.length;
        // 不断循环把绘制字母
        for (int i = 0; i < mLetters.length; i++) {
            String letter = mLetters[i];
            // 获取字体的宽度
            Paint mDefaultPaint = new Paint();
            mDefaultPaint.setTextSize(20);
            float measureTextWidth = mDefaultPaint.measureText(letter);
            // 获取内容的宽度
            int contentWidth = getWidth() - getPaddingLeft() - getPaddingRight();
            float x = getPaddingLeft() + (contentWidth - measureTextWidth) / 5;
            // 计算基线位置
            Paint.FontMetrics fontMetrics = mDefaultPaint.getFontMetrics();
            float baseLine = singleHeight / 2 + (singleHeight * i) + (fontMetrics.bottom - fontMetrics.top) / 2 - fontMetrics.bottom;
            // 画字母，后面onTouch的时候需要处理高亮
            canvas.drawText(letter, getMeasuredWidth() / 2, baseLine, mDefaultPaint);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        try {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    // 获取当前手指触摸的Y位置
                    float fingerY = event.getY();
                    float singleHeight = (float) mViewHeight / mLetters.length;
                    // 根据当前触摸的位置计算出，当前字母的位置
                    int position = (int) (fingerY / singleHeight);
                    // 如果和上个位置一样可以不处理
                    if (mLetters[position].equals(mCurrentTouchLetter)) {
                        return true;
                    }
                    // 记录当前触摸的位置
                    mCurrentTouchLetter = mLetters[position];
                    Log.d("mCurrentTouchLetter", mCurrentTouchLetter);
                    if (mOnTouch != null) {
                        mOnTouch.onTouch(mCurrentTouchLetter);
                    } else {
                        Log.d("mCurrentTouchLetter", "null");
                    }
                    break;
                case MotionEvent.ACTION_UP:
                    if (mOnTouch != null) {
                        mOnTouch.cancel();
                    } else {
                        Log.d("mCurrentTouchLetter", "null");
                    }
                    break;
            }
            // 重新绘制调用onDraw()方法
            invalidate();
            // 后面再去看事件分发的源码，先不要关注
            return true;
        } catch (Exception e) {

        }
        return true;
    }

    public void setListener(onTouch touch) {
        this.mOnTouch = touch;
    }

    interface onTouch {

        void onTouch(String name);

        void cancel();
    }
}
```