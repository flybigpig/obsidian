```

  
import android.content.Context  
import android.database.DataSetObservable  
import android.database.DataSetObserver  
import android.text.TextUtils  
import android.view.LayoutInflater  
import android.view.View  
import android.view.ViewGroup  
import android.widget.HorizontalScrollView  
import android.widget.ListAdapter  
import android.widget.RelativeLayout  
import android.widget.TextView  
import com.yto.pda.cwms.R  
import com.yto.pda.cwms.data.model.bean.InventoryListRsp  
import com.yto.pda.cwms.weight.MyHScrollView  
import java.text.SimpleDateFormat  
  
class StockGoodsDetailAdapter(ctx: Context, title: RelativeLayout) : ListAdapter {  
  
    private var context: Context? = null  
  
    private var mHead: RelativeLayout? = null  
  
    private var mInflater: LayoutInflater? = null  
  
    var dataBeans: java.util.ArrayList<InventoryListRsp.DataBean.RowsBean> = arrayListOf()  
  
    private val sdf = SimpleDateFormat("yyyy-MM-dd")  
  
    init {  
        context = ctx  
        mHead = title  
        mInflater = LayoutInflater.from(ctx)  
    }  
  
    private val mDataSetObservable = DataSetObservable()  
  
    override fun getCount(): Int {  
        return dataBeans.size  
    }  
  
    override fun getItem(position: Int): Any {  
        return dataBeans[position]  
    }  
  
    override fun getItemId(position: Int): Long {  
        return position.toLong()  
    }  
  
    override fun hasStableIds(): Boolean {  
        return false  
    }  
  
    override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {  
        var view: View  
        var holder = ViewHolder()  
        if (null == convertView) {  
            view  = mInflater!!.inflate(R.layout.include_head_inventory, null)  
  
            holder.scrollView = view!!.findViewById(R.id.horizontalScrollView1)  
            holder.tv_barCode = view!!.findViewById<View>(R.id.tv_barCode) as TextView  
            holder.tv_itemCode = view!!.findViewById<View>(R.id.tv_itemCode) as TextView  
            holder.tv_itemName = view!!.findViewById<View>(R.id.tv_itemName) as TextView  
            holder.tv_locationName = view!!.findViewById<View>(R.id.tv_locationName) as TextView  
            holder.tv_usableQuantity = view!!.findViewById<View>(R.id.tv_usableQuantity) as TextView  
            holder.tv_quantity = view!!.findViewById<View>(R.id.tv_quantity) as TextView  
            holder.tv_lockQuantity = view!!.findViewById<View>(R.id.tv_lockQuantity) as TextView  
            holder.tv_frozenQuantity = view!!.findViewById<View>(R.id.tv_frozenQuantity) as TextView  
            holder.tv_replLockQuantity = view!!.findViewById<View>(R.id.tv_replLockQuantity) as TextView  
            holder.tv_shouldReplQuantity = view!!.findViewById<View>(R.id.tv_shouldReplQuantity) as TextView  
            holder.tv_pieceNum = view!!.findViewById<View>(R.id.tv_pieceNum) as TextView  
            holder.tv_oddQuantity = view!!.findViewById<View>(R.id.tv_oddQuantity) as TextView  
            holder.tv_lotNumber = view!!.findViewById<View>(R.id.tv_lotNumber) as TextView  
            holder.tv_productDate = view!!.findViewById<View>(R.id.tv_produceDate) as TextView  
            holder.tv_expireDate = view!!.findViewById<View>(R.id.tv_expiredDate) as TextView  
            holder.tv_skuProperty = view!!.findViewById<View>(R.id.tv_skuProperty) as TextView  
            holder.tv_vector = view!!.findViewById<View>(R.id.tv_vector) as TextView  
  
  
            val headSrcrollView = mHead!!.findViewById<View>(R.id.horizontalScrollView1) as MyHScrollView  
            headSrcrollView.AddOnScrollChangedListener(OnScrollChangedListenerImp(holder.scrollView as MyHScrollView))  
            (holder.scrollView as MyHScrollView).AddOnScrollChangedListener(OnScrollChangedListenerImp(headSrcrollView))  
            view.tag = holder  
        } else {  
            view = convertView  
            holder = view.tag as ViewHolder  
        }  
        val dataBeanX: InventoryListRsp.DataBean.RowsBean = dataBeans[position]  
  
        holder.tv_barCode?.setText(dataBeanX.barCode?.toString()) // 商品条码  
  
        holder.tv_itemCode?.setText(dataBeanX.itemCode?.toString()) // 商品编码  
  
        holder.tv_itemName?.setText(dataBeanX.itemName?.toString()) // 商品名称  
  
        holder.tv_locationName?.setText(dataBeanX.locationName?.toString()) // 库位名称  
  
        holder.tv_usableQuantity?.setText(dataBeanX.usableQuantity?.toString() ) // 可用数量  
  
        holder.tv_quantity?.setText(dataBeanX.quantity?.toString() ) // 总库存  
  
        holder.tv_lockQuantity?.setText(dataBeanX.lockQuantity?.toString() ) // 出库锁定数量  
  
        holder.tv_frozenQuantity?.setText(dataBeanX.frozenQuantity?.toString() ) // 冻结数量  
  
        holder.tv_replLockQuantity?.setText(dataBeanX.replLockQuantity?.toString() ) // 出库锁定数量  
  
        holder.tv_shouldReplQuantity?.setText(dataBeanX.shouldReplQuantity?.toString() ) // 冻结数量  
  
        holder.tv_pieceNum?.setText(dataBeanX.pieceNum?.toString() ) // 整件数  
  
        holder.tv_oddQuantity?.setText(dataBeanX.oddQuantity?.toString() ) // 散件数  
  
        holder.tv_lotNumber?.setText(dataBeanX.lotNumber?.toString())  
  
        holder.tv_vector?.setText(dataBeanX.customerName?.toString())  
  
        if (!TextUtils.isEmpty(dataBeanX.productDate?.toString())){  
            holder.tv_productDate?.setText(sdf.format(sdf.parse(dataBeanX.productDate?.toString())))  
        }  
  
        if (!TextUtils.isEmpty(dataBeanX.expireDate?.toString())){  
            holder.tv_expireDate?.setText(sdf.format(sdf.parse(dataBeanX.expireDate?.toString())))  
        }  
        holder.tv_skuProperty?.setText(dataBeanX.skuProperty?.toString())  
  
        if (dataBeanX.isSelected) {  
            view!!.setBackgroundResource(R.color.drop_down_unselected)  
        } else {  
            view!!.setBackgroundResource(R.color.bg_gray)  
        }  
        return view  
    }  
  
    internal class OnScrollChangedListenerImp(var mScrollViewArg: MyHScrollView) :  
        MyHScrollView.OnScrollChangedListener {  
        override fun onScrollChanged(l: Int, t: Int, oldl: Int, oldt: Int) {  
            mScrollViewArg.smoothScrollTo(l, t)  
        }  
    }  
  
    override fun getItemViewType(position: Int): Int {  
        return 1  
    }  
  
    override fun getViewTypeCount(): Int {  
        return 1  
    }  
  
    override fun isEmpty(): Boolean {  
        return false  
    }  
  
    override fun areAllItemsEnabled(): Boolean {  
        return true  
    }  
  
    override fun isEnabled(position: Int): Boolean {  
        return true  
    }  
  
    fun setData(dataBeans: ArrayList<InventoryListRsp.DataBean.RowsBean>) {  
        this.dataBeans =dataBeans  
        notifyDataSetChanged()  
    }  
  
    internal class ViewHolder {  
        var tv_barCode: TextView? = null  
        var tv_itemCode: TextView? = null  
        var tv_itemName: TextView? = null  
        var tv_locationName: TextView? = null  
        var tv_usableQuantity: TextView? = null  
        var tv_quantity: TextView? = null  
        var tv_lockQuantity: TextView? = null  
        var tv_frozenQuantity: TextView? = null  
        var tv_replLockQuantity: TextView? = null  
        var tv_shouldReplQuantity: TextView? = null  
        var tv_pieceNum: TextView? = null  
        var tv_oddQuantity: TextView? = null  
        var tv_lotNumber: TextView? = null  
        var tv_productDate: TextView? = null  
        var tv_expireDate: TextView? = null  
        var tv_skuProperty: TextView? = null  
        var tv_vector: TextView? = null //货主  
        var scrollView: HorizontalScrollView? = null  
    }  
  
  
  
    override fun registerDataSetObserver(observer: DataSetObserver?) {  
        mDataSetObservable.registerObserver(observer)  
    }  
  
    override fun unregisterDataSetObserver(observer: DataSetObserver?) {  
        mDataSetObservable.unregisterObserver(observer)  
    }  
  
    /**  
     * Notifies the attached observers that the underlying data has been changed     * and any View reflecting the data set should refresh itself.     */    fun notifyDataSetChanged() {  
        mDataSetObservable.notifyChanged()  
    }  
  
    /**  
     * Notifies the attached observers that the underlying data is no longer valid     * or available. Once invoked this adapter is no longer valid and should     * not report further data set changes.     */    fun notifyDataSetInvalidated() {  
        mDataSetObservable.notifyInvalidated()  
    }  
  
  
}
```