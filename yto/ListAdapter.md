```
   
    class ListViewAdapter(context: Context, val resource: Int, data: ArrayList<String>) :
    
     ArrayAdapter<String>(context, resource, data) {
    

     override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
    
         val view: View
    
         val viewHolder: ViewHolder
    
         if (null == convertView) {
    
             view = LayoutInflater.from(parent.context).inflate(resource, parent, false)
    
             viewHolder = ViewHolder(view)
    
             view.tag = viewHolder
    
         } else {
    
             view = convertView
    
             viewHolder = view.tag as ViewHolder
    
         }
    

         val item = getItem(position)
    
         viewHolder.tv.text = item
    
         return view
    
     }
    

     inner class ViewHolder(view: View) {
    
         val tv: TextView = view.findViewById(R.id.tvLeft)
    
     }
    
}
```