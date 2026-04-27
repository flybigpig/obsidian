```
fun showSpan(v: TextView, list: ArrayList<String>, onClick: (pos: Int) -> Unit) {
    if (list == null || list.size <= 0) {
        return
    }

    var popupWindow = PopupWindow(requireActivity())
    popupWindow.apply {
        //入口参数配置
        contentView = layoutInflater.inflate(R.layout.popwindow_spinner_layout, null)
        width = v.width
        height = ViewGroup.LayoutParams.WRAP_CONTENT
        isFocusable = true

        //设置按钮
        val listview = contentView.findViewById<ListView>(R.id.listview)
        var adapter = context?.let { ArrayAdapter(it, R.layout.list_item_spinner_1, list) }
        //ListView设置适配器
        listview.adapter = adapter
        listview.setOnItemClickListener { _, _, position, _ ->
            onClick(position)
            dismiss()
        }
        //显示在按钮的下方
        showAsDropDown(v)
    }
}
```


```
asdasddasd 
```