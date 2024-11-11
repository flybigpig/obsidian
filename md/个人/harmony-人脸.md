## Harmony Next 接入

demo下载地址：[人脸核身控制台](https://console.cloud.tencent.com/faceid/access)（请到控制台自助接入列表页获取密码）。

1. 申请权限

a.在 model.json5文件中增加申请以下权限

  ```
  "requestPermissions": [

      {

        "name": "ohos.permission.INTERNET",

      },

      {

        "name" : "ohos.permission.CAMERA",

        "reason": "$string:app_name",

        "usedScene": {

          "abilities": [

            "EntryAbility"

          ],

  "when":"inuse"

        }

      }

    ]
```

﻿

2. WebviewController 的设置

调用 Web 前添加如下代码给 WebviewController 设置 ua。

@State controller: webview.WebviewController = new webview.WebviewController();

3. 创建 Web 组件

```
Web({

        src: 'https://kyc.qcloud.com/s/web/h5/#/entry', //设置加载的实时模式刷脸h5页面的url地址

        controller: this.controller  //给web设置controller

      })

        .onControllerAttached(() => {

          this.controller.setCustomUserAgent(this.controller.getCustomUserAgent() + ';kyc/h5face;hoskyc/2.0') //设置ua

        })

        .fileAccess(true)//web的配置，不能少

        .javaScriptAccess(true)//web的配置，不能少
```

﻿

4. web 组件配置实时模式

```
onPermissionRequest((event) => { // 终端页面收到h5刷脸实时模式的请求

          if (event) {

            this.checkPermission().then(() => { //1. 终端申请相机权限成功

              event.request.grant(event.request.getAccessibleResource()) // 2. 授权h5页面

            }).catch((error: BusinessError) => { // 终端申请相机权限失败

              console.error(TAG, "申请权限异常" + error.message)

              event.request.deny() //2.告诉h5页面没有权限

            })

          }

        })
```

// 注意：checkersmission()是申请相机的方法，接入方可以根据自身业务需求发挥，demo的这个方法仅供参考。

5. web 组件配置传统录制模式

```
.onShowFileSelector((event) => { //终端页面收到h5刷脸传统录制模式的请求

          if (event) {

            let result = picCamera().then(result => { //1.打开系统相机

              let str: string[] = [result.resultUri]

              event.result.handleFileList(str) // 2. 把录制的视频传给h5页面

            }).catch((error: BusinessError) => { //打开系统相机异常

              console.error(TAG, "打开相机异常" + error.message)

            });

          }

          return true;

        })
```