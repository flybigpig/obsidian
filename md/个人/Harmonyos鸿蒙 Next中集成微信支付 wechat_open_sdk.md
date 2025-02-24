**Harmonyos鸿蒙 Next微信支付教程**：[https://www.itying.com/goods-1204.html](https://www.itying.com/goods-1204.html)

![22_20241128105945.png](http://bbs.itying.com/public/upload/d9b5ce10-ad34-11ef-81a9-331a04a179e7.png)

### 一、Harmonyos鸿蒙 Next 微信APP支付前的准备工作，以及需要获取内容

**1、准备工作：** 必须要有企业营业执照、对公账户。

**2、需要获取内容:**

**APPID**：应用APPID（必须配置，开户邮件中可查看）

**MCHID**：微信支付商户号（必须配置，开户邮件中可查看）

**KEY**：API密钥，参考开户邮件设置（必须配置，登录商户平台自行设置）

**商户API证书**：商户API证书用于apiv3的支付

### 二、Harmonyos鸿蒙 Next调用微信支付调用统一下单接口 生成预支付信息

```
package main

import (
"context"
"fmt"
"io/ioutil"
"net/http"
"time"

"github.com/gin-gonic/gin"
"github.com/go-pay/gopay"
"github.com/go-pay/gopay/wechat/v3"
"github.com/go-pay/xlog"
)

var (
mchID    string = "1497637762"                               // 商户号
serialNo string = "47A66357114B9B3AE344A537BCA0C78156554017" // 商户证书序列号
apiV3Key string = "zhongyuantengitying6666666666666"         // 商户APIv3密钥
appID    string = "wx5881fa2638a2ca60"                       // 应用ID
)

func main() {

// 读取私钥文件
privateKey, err := ioutil.ReadFile("./cert/apiclient_key.pem")
if err != nil {
xlog.Error(err)
return
}
// 初始化微信V3客户端
client, err := wechat.NewClientV3(mchID, serialNo, apiV3Key, string(privateKey))
if err != nil {
xlog.Error(err)
return
}

// 启用自动同步返回验签，并定时更新微信平台API证书
err = client.AutoVerifySign()
if err != nil {
xlog.Error(err)
return
}

// 打开Debug开关，输出日志
// client.DebugSwitch = gopay.DebugOn

// 调用统一下单API（APP支付）
tradeNo := "12345" + time.Now().Format("20060102150405")
err = createAppOrder(client, appID, tradeNo)
if err != nil {
xlog.Error(err)
}
}

func createAppOrder(client *wechat.ClientV3, appID, tradeNo string) error {
expire := time.Now().Add(10 * time.Minute).Format(time.RFC3339)

bm := make(gopay.BodyMap)
bm.Set("appid", appID). // 应用ID
Set("mchid", mchID). // 商户号
Set("description", "测试app支付商品").
Set("out_trade_no", tradeNo).
Set("time_expire", expire).
Set("notify_url", "https://www.itying.com").
SetBodyMap("amount", func(bm gopay.BodyMap) {
bm.Set("total", 1).
Set("currency", "CNY")
})

ctx := context.Background()
wxRsp, err := client.V3TransactionApp(ctx, bm)
if err != nil {
return err
}

fmt.Println("-------------wxRsp.Code----------------")
fmt.Println(wxRsp.Code)
fmt.Println(gopay.SUCCESS)
print(wxRsp.Response.PrepayId)

if wxRsp.Code == 0 {
// 获取支付所需的签名
appSign, err := client.PaySignOfApp(appID, wxRsp.Response.PrepayId)
if err != nil {
return err
}

// 打印支付所需的参数
fmt.Printf("appId: %s\n", appSign.Appid)
fmt.Printf("timeStamp: %s\n", appSign.Timestamp)
fmt.Printf("nonceStr: %s\n", appSign.Noncestr)
fmt.Printf("package: %s\n", appSign.Package)
fmt.Printf("signType: %s\n", appSign.Sign)
fmt.Printf("Prepayid: %s\n", appSign.Prepayid)
fmt.Printf("Partnerid: %s\n", appSign.Partnerid)

// 在这里，你可以将支付参数传递给前端，以便调起微信支付
} else {
xlog.Errorf("wxRsp: %s", wxRsp.Error)
return fmt.Errorf(wxRsp.Error)
}

return nil
}

func handleNotify(c *gin.Context) {
notifyReq, err := wechat.V3ParseNotify(c.Request)
if err != nil {
c.JSON(http.StatusBadRequest, gin.H{"code": gopay.FAIL, "message": err.Error()})
return
}

client := getWechatClient() // 你需要实现这个函数来获取已经初始化的wechat.ClientV3实例

// 验证异步通知的签名
certMap := client.WxPublicKeyMap()
err = notifyReq.VerifySignByPKMap(certMap)
if err != nil {
c.JSON(http.StatusBadRequest, gin.H{"code": gopay.FAIL, "message": err.Error()})
return
}

// 处理支付结果
// notifyReq.Resource 包含了支付结果的信息，你可以根据需要进行解析和处理

// 回复微信服务器
c.JSON(http.StatusOK, &wechat.V3NotifyRsp{Code: gopay.SUCCESS, Message: "成功"})
}

func getWechatClient() *wechat.ClientV3 {
// 读取私钥文件
privateKey, err := ioutil.ReadFile("apiclient_key.pem")
if err != nil {
xlog.Error(err)
return nil
}

// 初始化微信V3客户端（这里应该使用你的实际配置）
client, err := wechat.NewClientV3("your_mch_id", "your_serial_no", "your_api_v3_key", string(privateKey))
if err != nil {
xlog.Error(err)
return nil
}

// 启用自动同步返回验签，并定时更新微信平台API证书
err = client.AutoVerifySign()
if err != nil {
xlog.Error(err)
return nil
}

return client
}
```

### 三、Harmonyos鸿蒙 Next调用微信支付调用统一下单接口 生成预支付信息

### 二、Harmonyos Next中集成 **wechat\_open\_sdk**

[https://ohpm.openharmony.cn/#/cn/detail/](https://ohpm.openharmony.cn/#/cn/detail/)[@tencent](http://bbs.itying.com/user/tencent)%2Fwechat\_open\_sdk

[https://developers.weixin.qq.com/doc/oplatform/Mobile\_App/Access\_Guide/ohos.html](https://developers.weixin.qq.com/doc/oplatform/Mobile_App/Access_Guide/ohos.html)

#### 配置网络权限

```
"requestPermissions": [
      {
        "name": "ohos.permission.INTERNET" //网络
      }
 ]
```

#### 安装微信支付SDK

[https://ohpm.openharmony.cn/#/cn/detail/](https://ohpm.openharmony.cn/#/cn/detail/)[@tencent](http://bbs.itying.com/user/tencent)%2Fwechat\_open\_sdk

```
ohpm install @ohos/axios

ohpm i [@tencent](/user/tencent)/wechat_open_sdk
```

#### Harmonyos鸿蒙 Next调用 wechat\_open\_sdk 实现支付

```
import { bundleManager, common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { HttpGet } from '../common/HttpUtil';
import { ResponseModel } from '../model/ResponseModel';
import * as wxopensdk from '[@tencent](/user/tencent)/wechat_open_sdk';
import { OnWXResp, WXApi, WXEventHandler } from '../model/WXApiWrap';
interface OrderInfoInterface {
  appid: string
  noncestr: string
  package: string
  partnerid: string
  prepayid: string
  timestamp: string | undefined
  sign: string
}
@Entry
@Component
struct Index {
  @State message: string = 'Hello World';
  private wxApi = WXApi
  private wxEventHandler = WXEventHandler
  @State payResultText: string = ''
  private onWXResp: OnWXResp = (resp) => {
    console.log("onWXResp 执行");
    this.payResultText = JSON.stringify(resp, null, 2)
  }

  aboutToAppear(): void {
    this.wxEventHandler.registerOnWXRespCallback(this.onWXResp)
  }

  aboutToDisappear(): void {
    this.wxEventHandler.unregisterOnWXRespCallback(this.onWXResp)
  }

  doWxPay = () => {
    let apiUri: string = `https://xxxxx.itying.com/wxpay`
    HttpGet(apiUri).then((response: ResponseModel) => {
      let orderInfo: OrderInfoInterface = response.result as OrderInfoInterface
      this.wxPay(orderInfo)
    })
  }
  wxPay = async (orderInfo: OrderInfoInterface) => {

    console.log(orderInfo.appid);
    console.log(orderInfo.partnerid);
    console.log(orderInfo.package);
    console.log(orderInfo.prepayid);
    console.log(orderInfo.noncestr);
    console.log(orderInfo.timestamp);
    console.log(orderInfo.sign);

    let req = new wxopensdk.PayReq
    req.partnerId = orderInfo.partnerid
    req.appId = orderInfo.appid
    req.packageValue = orderInfo.package
    req.prepayId = orderInfo.prepayid
    req.nonceStr = orderInfo.noncestr
    req.timeStamp = orderInfo.timestamp
    req.sign = orderInfo.sign
    req.extData = 'extData'

    console.log("---------");

    let finished = await this.wxApi.sendReq(getContext(this) as common.UIAbilityContext, req)
    console.log("send request finished: ", finished)

  }
  build() {
    Column() {
      Text(this.payResultText)
        .id('HelloWorld')
        .fontSize(50)
        .fontWeight(FontWeight.Bold)

      Button("执行微信支付").onClick(() => {
        this.doWxPay()
      }).width('80%')

    }
    .height('100%')
    .width('100%')
    .layoutWeight(1)
    .justifyContent(FlexAlign.Center)

  }
}
```

![22_20241128105945.png](http://bbs.itying.com/public/upload/d9b5ce10-ad34-11ef-81a9-331a04a179e7.png)

**Harmonyos鸿蒙 Next微信支付教程**：[https://www.itying.com/goods-1204.html](https://www.itying.com/goods-1204.html)