```
package com.yto.pda.cwms.ui.adapter  
  
  
  
class MyExpandAdapter(context: Context, headData: ArrayList<String>, listData: ArrayList<ArrayList<ExpandItemBean>>) :  
    BaseExpandableListAdapter() {  
  
    var head: ArrayList<String> = arrayListOf()  
    var list: ArrayList<ArrayList<ExpandItemBean>> = arrayListOf()  
    lateinit var ctx: Context  
  
  
    private var method: (data: ExpandItemBean, view: View, groupPposition: Int, childPosition: Int) -> Unit =  
        { _: ExpandItemBean, _: View, _: Int, _: Int -> }  
  
    init {  
        head = headData  
        list = listData  
        ctx = context  
    }  
  
    override fun getGroupCount(): Int {  
        return head.size  
    }  
  
    override fun getChildrenCount(groupPosition: Int): Int {  
        if (groupPosition < 0 || groupPosition >= this.list.size)  
            return 0;  
        return list.get(groupPosition).size;  
    }  
  
    override fun getGroup(groupPosition: Int): String {  
        return head[groupPosition]  
    }  
  
    override fun getChild(groupPosition: Int, childPosition: Int): ExpandItemBean {  
        return list[groupPosition][childPosition]  
    }  
  
    override fun getGroupId(groupPosition: Int): Long {  
        return groupPosition.toLong()  
    }  
  
    override fun getChildId(groupPosition: Int, childPosition: Int): Long {  
        return getCombinedChildId(groupPosition.toLong(), childPosition.toLong());  
    }  
  
  
    override fun hasStableIds(): Boolean {  
        return true  
    }  
  
    override fun getGroupView(groupPosition: Int, isExpanded: Boolean, convertView: View?, parent: ViewGroup?): View {  
        //获取文本     
        val text: String = head[groupPosition]  
        if (convertView == null) {  
            val layoutInflater: LayoutInflater = LayoutInflater.from(ctx)  //使用这个来载入界面  
            var view = layoutInflater.inflate(R.layout.plain_head_text, null)  
  
            val tv = view!!.findViewById(R.id.tv_title) as TextView  
            tv.text = text  
            return view!!  
        }  
        val tv = convertView!!.findViewById(R.id.tv_title) as TextView  
        tv.text = text  
        return convertView  
    }  
  
    override fun getChildView(  
        groupPosition: Int, childPosition: Int, isLastChild: Boolean, convertView: View?, parent: ViewGroup?  
    ): View {  
        if (convertView == null) { //convert在运行中会重用，如果不为空，则表明不用重新获取  
            val layoutInflater: LayoutInflater = LayoutInflater.from(ctx)  //使用这个来载入界面  
            var view = layoutInflater.inflate(R.layout.plain_text, null)  
            Log.d("TAGGGGGGGGGG1", groupPosition.toString() + " " + childPosition.toString())  
            dealWith(view, groupPosition, childPosition)  
            return view!!  
        }  
        Log.d("TAGGGGGGGGGG2", groupPosition.toString() + " " + childPosition.toString())  
        dealWith(convertView, groupPosition, childPosition)  
        return convertView!!  
    }  
  
    private fun dealWith(view: View?, groupPosition: Int, childPosition: Int) {  
  
        var bean = list[groupPosition][childPosition] as ExpandItemBean  
        Log.d("TAGGG", bean.toString())  
        val tvName = view!!.findViewById(R.id.tv_name) as TextView  
        val tvContent = view!!.findViewById(R.id.tv_content) as TextView  
        var switch = view!!.findViewById<Switch>(R.id.sw_btn) as Switch  
        if (!TextUtils.isEmpty(bean.unit)) {  
            tvName.text = bean.name + "(" + bean.unit + ")"  
        } else {  
            tvName.text = bean.name  
        }  
        Log.d("TAGGG", bean.name.toString())  
        if (!bean.check!!) {  
            tvContent.visibility = View.VISIBLE  
            switch.visibility = View.GONE  
            var status = bean.status  
  
            tvContent.text = bean.content.toString()  
            if (bean.name != "商品编码") {  
                tvContent.setTextColor(ctx.resources.getColor(R.color.colorPrimary))  
            } else {  
                tvContent.setTextColor(ctx.resources.getColor(R.color.colorBlack333))  
            }  
  
            view!!.findViewById<LinearLayout>(R.id.ll_layout).setOnClickListener {  
                Log.d("TAGGGGGG", groupPosition.toString() + " " + childPosition.toString())  
                method(  
                    bean,  
                    view!!.findViewById<LinearLayout>(R.id.ll_layout),  
                    groupPosition,  
                    childPosition  
                )  
            }  
        } else {  
            tvContent.visibility = View.GONE  
            switch.visibility = View.VISIBLE  
            var flag = bean.status.toString().toBoolean()  
            switch.isChecked = flag  
        }  
  
    }  
  
    override fun isChildSelectable(groupPosition: Int, childPosition: Int): Boolean {  
        return true;  
    }  
  
    override fun onGroupCollapsed(groupPosition: Int) {  
        super.onGroupCollapsed(groupPosition)  
  
    }  
  
    override fun onGroupExpanded(groupPosition: Int) {  
        super.onGroupExpanded(groupPosition)  
  
    }  
  
    fun setChildClick(method: (data: ExpandItemBean, view: View, groupPosition: Int, childPosition: Int) -> Unit) {  
        this.method = method  
    }  
  
}
```
