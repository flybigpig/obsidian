```
package com.yto.pda.cwms.util;  
  
import org.json.JSONArray;  
import org.json.JSONException;  
import org.json.JSONObject;  
  
import java.util.ArrayList;  
import java.util.HashMap;  
import java.util.Iterator;  
import java.util.List;  
import java.util.Map;  
  
public class JsonMap {  
    /**  
     * 将json 数组转换为Map 对象  
     *  
     * @param jsonString  
     * @return  
     */  
  
    public static HashMap<String, Object> getMap(String jsonString) {  
        JSONObject jsonObject;  
        try {  
            jsonObject = new JSONObject(jsonString);  
            @SuppressWarnings("unchecked") Iterator<String> keyIter = jsonObject.keys();  
            String key;  
            Object value;  
            HashMap<String, Object> valueMap = new HashMap<String, Object>();  
            while (keyIter.hasNext()) {  
                key = (String) keyIter.next();  
                value = jsonObject.get(key);  
                valueMap.put(key, value);  
            }  
            return valueMap;  
        } catch (JSONException e) {  
            e.printStackTrace();  
        }  
  
        return null;  
  
    }  
  
  
    /**  
     * 把json 转换为ArrayList 形式  
     *  
     * @return  
     */  
    public static List<Map<String, Object>> getList(String jsonString) {  
        List<Map<String, Object>> list = null;  
        try {  
            JSONArray jsonArray = new JSONArray(jsonString);  
            JSONObject jsonObject;  
            list = new ArrayList<Map<String, Object>>();  
            for (int i = 0; i < jsonArray.length(); i++) {  
                jsonObject = jsonArray.getJSONObject(i);  
                list.add(getMap(jsonObject.toString()));  
            }  
        } catch (Exception e) {  
            e.printStackTrace();  
        }  
        return list;  
  
    }  
}
```
```
```


```
{
	"YT7502210691521": [{
		"logisticsName": "圆通快递",
		"itemName": "南极人102",
		"quantity": 1,
		"orderId": "OB2410281742300098000007",
		"receiverName": "XyrTbK18GrRrsuPm9otnsA==",
		"itemCode": "NANJIREN102",
		"expressCode": "YT7502210691521",
		"barCode": "NANJIREN102"
	}],
	"totalCount": 4,
	"YT7502729289408": [{
		"logisticsName": "圆通快递",
		"itemName": "南极人102",
		"quantity": 2,
		"orderId": "OB2410281742300098000007",
		"receiverName": "XyrTbK18GrRrsuPm9otnsA==",
		"itemCode": "NANJIREN102",
		"expressCode": "YT7502729289408",
		"barCode": "NANJIREN102"
	}],
	"YT7502763604340": [{
		"logisticsName": "圆通快递",
		"itemName": "南极人103",
		"quantity": 1,
		"orderId": "OB2410281742300098000007",
		"receiverName": "XyrTbK18GrRrsuPm9otnsA==",
		"itemCode": "NANJIREN103",
		"expressCode": "YT7502763604340",
		"barCode": "NANJIREN103"
	}, {
		"logisticsName": "圆通快递",
		"itemName": "南极人104",
		"quantity": 1,
		"orderId": "OB2410281742300098000007",
		"receiverName": "XyrTbK18GrRrsuPm9otnsA==",
		"itemCode": "NANJIREN104",
		"expressCode": "YT7502763604340",
		"barCode": "NANJIREN104"
	}],
	"YT7502408626645": [{
		"logisticsName": "顺心捷达",
		"itemName": "南极人102",
		"quantity": 1,
		"orderId": "OB2410281742300098000007",
		"receiverName": "XyrTbK18GrRrsuPm9otnsA==",
		"itemCode": "NANJIREN102",
		"expressCode": "YT7502408626645",
		"barCode": "NANJIREN102"
	}]
}
```


```
 fun selectPackRecordPda(pickId: String, data: MultipleReviewResponse) {  
  //  customerId=300113&orderId=YT7502210691521&warehouseId=300098&page=1&limit=500&currPackedNum=0  
      

        val request: HashMap<String, Any> = HashMap()  
        request["customerId"] = data.order.customerId  
        request["orderId"] = data.order.expressCode  
        request["warehouseId"] = CacheUtil.getWareHouse()!!.warehouseId  
        request["page"] = 1  
        request["limit"] = 500  
        request["currPackedNum"] = 0  
        requestNoCheck(  
            { apiService.selectPackRecordPda(request) },  
            { it ->  
                var json = Gson().toJson(it)  
                var map = JsonMap.getMap(json)  
                recordData.value = map  
                map.map { (key, value) -> Log.d("TAGG", "$key: $value") }  
            },  
            { it ->  
                recordData.value = java.util.HashMap()  
            },  
            true,            "数据请求中......"  
        )  
  
    }
```

```
map.map {  
    var item = PackedRecordData()  
    item.label = it.key.toString()  
    val listType = object : TypeToken<List<PackedRecordData.DataBean>>() {}.type  
    var value: ArrayList<PackedRecordData.DataBean> = Gson().fromJson(it.value.toString(), listType)  
    item.value = value  
    list.add(item)  
}
```