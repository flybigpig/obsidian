```

  
import android.text.TextUtils  
  
/**  
 * Created by BaJie on 2018/1/17 0017. */class ItemV2Rsp {  
    var data: ArrayList<DataBean>? = arrayListOf()  
  
    class DataBean {  
        var barCode: String? = null  
        var itemName: String? = null  
        var itemCode: String? = null  
        var itemId: String? = null  
        var length: String? = null  
            get() {  
                return if (TextUtils.isEmpty(length)) {  
                    ""  
                } else {  
                    length  
                }  
            }  
        var width: String? = null  
            get() {  
                return if (TextUtils.isEmpty(width)) {  
                    ""  
                } else {  
                    width  
                }  
            }  
        var height: String? = null  
            get() {  
                return if (TextUtils.isEmpty(height)) {  
                    ""  
                } else {  
                    height  
                }  
            }  
        var cube: String? = null  
            get() {  
                return if (TextUtils.isEmpty(cube)) {  
                    ""  
                } else {  
                    cube  
                }  
            }  
        var grossWeight: String? = null  
        var isBatchMgmt = false  
        var isShelfLifeMgmt = false  
        var lotnumberRule: String? = null  
        var shelfLife: String? = null  
        var boxBarCode: String? = null  
        var sbBarCode: String? = null  
        var boxQuantity: Int = 0  
        var secondQuantity: Int = 0  
        var boxLength: String? = null  
        var boxHeight: String? = null  
        var boxWidth: String? = null  
        var boxWeight: String? = null  
        var boxCube: String? = null  
        var floorBoxRate: String? = null  
        var floorPallet: String? = null  
        var itemGroupName: String? = null  
        var skuProperty: String? = null  
            get() {  
                return if (TextUtils.isEmpty(skuProperty)) {  
                    ""  
                } else {  
                    skuProperty  
                }  
            }  
        var stockUnit: String? = null  
        var netWeight: String? = null  
        var lockupLifecycle: Int = 0  
        var rejectLifecycle: Int = 0  
        var locationAgeWarning: Int = 0  
        var adventLifecycle: Int = 0  
  
        var isOutCollectSn: Boolean = false  
        var isInCollectSn: Boolean = false  
    }  
  
  
}
```


