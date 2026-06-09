HiCar SDK 接入是个比较系统的工程，我给你梳理一份完整的车机端 + 手机端联调流程。

---

## 🚗 HiCar SDK 整体架构与联调流程

### 一、HiCar 通信架构

```
┌──────────────────────┐         ┌──────────────────────┐
│   📱 手机端 (HiCar SDK) │         │  🚙 车机端 (HiCar Agent) │
│                      │         │                      │
│  HiCar App / 集成SDK │         │  HiCar 协议栈（鸿蒙） │
│  HiCar Service      │         │  HiCar Service       │
│  USB/Wi-Fi/BT 驱动   │◄───────►│  USB/Wi-Fi/BT Host   │
│                      │ USB/WiFi│  Display/Audio/Car   │
└──────────────────────┘         └──────────────────────┘
        │                                    │
        │              ┌─────────┐          │
        └─────────────►│  HiCar  │◄─────────┘
                       │  Cloud  │  (HiCar 账号、车辆绑定)
                       │  Service│
                       └─────────┘
```

**通信通道**：
- **USB**（有线，稳定）
- **Wi-Fi 5GHz**（无线，高带宽）
- **蓝牙**（辅助通道，做发现/认证）

---

### 二、HiCar 接入前置准备

#### 1. 注册华为开发者账号 & 申请 HiCar 权限
```
https://developer.huawei.com/consumer/cn/hicar
```

#### 2. 申请材料
- 公司资质（OEM/Tier1 厂商）
- 车机硬件规格
- 安全合规证明（ISO 21434 等）
- App 集成计划

#### 3. 获取密钥文件
- **HiCar SDK 包**（jar/so/framework）
- **App ID**（HiCar 分配）
- **App Secret**（服务器端签名用）
- **Bundle ID / Package Name**（车机端 + 手机端，需注册）
- **证书指纹**（SHA-256 校验）

---

### 三、车机端（鸿蒙）集成流程

#### 1. 工程结构准备

```json5
// entry/build-profile.json5 - 鸿蒙应用配置
{
  "app": {
    "signingConfigs": [],
    "products": [
      {
        "name": "default",
        "signingConfig": "default",
        "compatibleSdkVersion": "4.0.0",
        "runtimeOS": "HarmonyOS",
        "buildOption": {
          "strictMode": {
            "caseSensitiveCheck": true
          }
        }
      }
    ],
    "buildModeSet": [
      { "name": "debug" },
      { "name": "release" }
    ]
  },
  "modules": [
    {
      "name": "entry",
      "type": "entry",
      "deviceTypes": ["car"],   // ← 关键：声明车机端
      "srcPath": "entry"
    }
  ]
}
```

#### 2. 配置依赖（oh-package.json5）

```json5
{
  "dependencies": {
    "@hms.hicar.core": "1.0.0",          // HiCar 核心
    "@hms.hicar.service": "1.0.0",       // HiCar 服务
    "@hms.hicar.carcontrol": "1.0.0",    // 车控（需 TBOX 配合）
    "@hms.hicar.media": "1.0.0",         // 音视频
    "@hms.hicar.nav": "1.0.0"            // 导航
  }
}
```

#### 3. 配置权限（module.json5）

```json5
{
  "module": {
    "requestPermissions": [
      { "name": "ohos.permission.INTERNET" },
      { "name": "ohos.permission.GET_WIFI_INFO" },
      { "name": "ohos.permission.USE_BLUETOOTH" },
      { "name": "ohos.permission.SYSTEM_LIGHT_CONTROL" },  // 屏幕控制
      { "name": "ohos.permission.HICAR_CAR_CONTROL" },     // HiCar 车控
      { "name": "ohos.permission.HICAR_DISPLAY" }          // 投屏权限
    ],
    "abilities": [
      {
        "name": "HiCarEntryAbility",
        "srcEntry": "./ets/entryability/HiCarEntryAbility.ets",
        "launchType": "singleton",
        "metadata": [
          {
            "name": "hicar_metadata",
            "resource": "$profile:hicar_config"
          }
        ]
      }
    ]
  }
}
```

#### 4. HiCar 配置文件（resources/base/profile/hicar_config.json）

```json
{
  "hicar": {
    "appId": "your_app_id_from_huawei",
    "version": "1.0.0",
    "deviceTypes": ["car"],
    "supportedConnection": ["USB", "WIFI_P2P"],
    "protocols": {
      "mirror": true,        // 投屏
      "carControl": true,    // 车控
      "hvac": true,          // 空调
      "media": true          // 媒体
    }
  }
}
```

---

#### 5. 车机端 HiCar 服务实现

```typescript
// entry/src/main/ets/services/HiCarManager.ets
import hms from '@hms.hicar.core';
import deviceInfo from '@ohos.deviceInfo';
import { BusinessError } from '@ohos.base';
import Logger from '../utils/Logger';

const TAG = 'HiCarManager';

export class HiCarManager {
  private static instance: HiCarManager;
  private isInitialized: boolean = false;
  private connectedDeviceId: string = '';
  private hicarListeners: Set<HiCarEventListener> = new Set();

  public static getInstance(): HiCarManager {
    if (!HiCarManager.instance) {
      HiCarManager.instance = new HiCarManager();
    }
    return HiCarManager.instance;
  }

  // ========== 1. 初始化 HiCar SDK ==========
  async init(): Promise<boolean> {
    if (this.isInitialized) return true;
    try {
      const appId = 'your_hicar_app_id';        // 从 hicar_config.json 读取
      const appSecret = 'your_app_secret';      // 服务器端签名用
      const deviceId = deviceInfo.udid;

      await hms.hicar.init({
        appId: appId,
        appSecret: appSecret,
        deviceId: deviceId,
        deviceType: 'CAR',                       // 声明车机端
        osVersion: deviceInfo.osFullName,
        sdkVersion: '1.0.0'
      });
      this.isInitialized = true;
      this.registerListeners();
      Logger.info(TAG, 'HiCar SDK initialized');
      return true;
    } catch (err) {
      const e = err as BusinessError;
      Logger.error(TAG, `HiCar init failed: ${e.code} ${e.message}`);
      return false;
    }
  }

  // ========== 2. 注册事件监听 ==========
  private registerListeners(): void {
    // 手机连接
    hms.hicar.on('deviceConnected', (event) => {
      Logger.info(TAG, `Device connected: ${JSON.stringify(event)}`);
      this.connectedDeviceId = event.deviceId;
      this.notifyListeners({ type: 'CONNECT', data: event });
    });

    // 手机断开
    hms.hicar.on('deviceDisconnected', (event) => {
      Logger.info(TAG, `Device disconnected: ${event.deviceId}`);
      this.connectedDeviceId = '';
      this.notifyListeners({ type: 'DISCONNECT', data: event });
    });

    // 投屏请求
    hms.hicar.on('projectionRequest', (event) => {
      Logger.info(TAG, `Projection request: ${event.sessionId}`);
      this.notifyListeners({ type: 'PROJECTION', data: event });
    });

    // 车控指令（来自手机 HiCar App）
    hms.hicar.on('carControlCommand', (event) => {
      Logger.info(TAG, `Car control command: ${event.action}`);
      this.handleCarControl(event);
    });
  }

  // ========== 3. 处理车控指令（关键安全点） ==========
  private async handleCarControl(event: any): Promise<void> {
    const { action, params, signature, certHash } = event;
    
    // 安全验证：必须是合法手机 + 合法签名
    const isValid = await this.verifyControlSignature(action, params, signature, certHash);
    if (!isValid) {
      Logger.error(TAG, `Invalid control signature, reject command: ${action}`);
      return;
    }

    // 转发到 TBOX 执行
    const tbox = new TboxPkiClient();
    await tbox.executeControlCommand(action, params);
  }

  // ========== 4. 暴露车机能力给手机 ==========
  async publishCarCapabilities(): Promise<void> {
    const capabilities = {
      hvac: { support: true, modes: ['cool', 'heat', 'auto'] },
      doors: { support: true, count: 4 },
      windows: { support: true, count: 4 },
      engine: { support: true, actions: ['start', 'stop'] },
      media: { support: true, formats: ['mp3', 'aac', 'flac'] },
      nav: { support: true }
    };
    await hms.hicar.publishCapabilities(capabilities);
  }

  // ========== 5. 投屏接收 ==========
  async startProjection(sessionId: string): Promise<void> {
    await hms.hicar.startProjection({
      sessionId: sessionId,
      displayId: 'main_display',
      resolution: '1920x720',
      fps: 30
    });
  }

  private async verifyControlSignature(action: string, params: any, signature: string, certHash: string): Promise<boolean> {
    // 调用 TBOX-PKI 验证签名
    return true;  // 实际实现见安全 SDK 部分
  }

  private notifyListeners(event: HiCarEvent): void {
    this.hicarListeners.forEach(l => l.onEvent(event));
  }

  public addListener(listener: HiCarEventListener): void {
    this.hicarListeners.add(listener);
  }
}

export interface HiCarEvent {
  type: 'CONNECT' | 'DISCONNECT' | 'PROJECTION' | 'CAR_CONTROL';
  data: any;
}

export interface HiCarEventListener {
  onEvent(event: HiCarEvent): void;
}
```

#### 6. 车机端入口 Ability

```typescript
// entry/src/main/ets/entryability/HiCarEntryAbility.ets
import UIAbility from '@ohos.app.ability.UIAbility';
import window from '@ohos.window';
import { HiCarManager } from '../services/HiCarManager';

export default class HiCarEntryAbility extends UIAbility {
  async onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): Promise<void> {
    console.log('HiCarEntryAbility onCreate');
    // 启动时初始化 HiCar
    await HiCarManager.getInstance().init();
    await HiCarManager.getInstance().publishCarCapabilities();
  }

  onDestroy(): void {
    console.log('HiCarEntryAbility onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Index', (err, data) => {
      if (err.code) {
        console.error(`Failed to load content: ${err.message}`);
        return;
      }
      console.log('WindowStage created');
    });
  }
}
```

---

### 四、手机端（Android）集成流程

#### 1. 添加依赖（build.gradle）

```gradle
// 项目级 build.gradle
buildscript {
    dependencies {
        classpath 'com.huawei.agconnect:agcp:1.6.5.300'
    }
}

// app/build.gradle
dependencies {
    implementation 'com.huawei.hms:hicar-base:6.12.0.301'
    implementation 'com.huawei.hms:hicar-control:6.12.0.301'
    implementation 'com.huawei.hms:hicar-media:6.12.0.301'
    implementation 'com.huawei.hms:hicar-discovery:6.12.0.301'
}
```

#### 2. AndroidManifest 配置

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <!-- HiCar 权限 -->
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
    <uses-permission android:name="android.permission.CHANGE_WIFI_STATE" />
    <uses-permission android:name="com.huawei.hicar.permission.SERVICE" />
    
    <application
        android:label="@string/app_name"
        android:icon="@mipmap/ic_launcher">
        
        <!-- HiCar Service -->
        <service
            android:name=".hicar.HiCarService"
            android:exported="true"
            android:permission="com.huawei.hicar.permission.SERVICE">
            <intent-filter>
                <action android:name="com.huawei.hicar.service" />
            </intent-filter>
        </service>
        
    </application>
</manifest>
```

#### 3. 手机端 HiCar 服务

```java
// app/src/main/java/com/example/hicar/HiCarService.java
package com.example.hicar;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import com.huawei.hms.hicar.HCar;
import com.huawei.hms.hicar.callback.ConnectionCallback;
import com.huawei.hms.hicar.callback.CarControlCallback;
import com.huawei.hms.hicar.callback.ProjectionCallback;
import com.huawei.hms.hicar.model.CarDevice;
import com.huawei.hms.hicar.model.ControlCommand;
import com.huawei.hms.api.ConnectionResult;
import com.huawei.hms.api.HuaweiApiClient;

public class HiCarService extends Service {
    private static final String TAG = "HiCarService";
    private static final String APP_ID = "your_hicar_app_id";
    private HuaweiApiClient client;
    private HCar hCar;

    @Override
    public void onCreate() {
        super.onCreate();
        initHiCar();
    }

    private void initHiCar() {
        client = new HuaweiApiClient.Builder(this)
            .addApi(HCar.API)              // 关键：添加 HCar API
            .addConnectionCallbacks(connectionCallback)
            .addOnConnectionFailedListener(connectionFailedListener)
            .build();
        client.connect();

        hCar = HCar.getInstance(client);
    }

    private ConnectionCallbacks connectionCallback = new ConnectionCallbacks() {
        @Override
        public void onConnected() {
            Log.i(TAG, "HCar API connected");
            discoverCars();
        }

        @Override
        public void onConnectionSuspended(int cause) {
            Log.w(TAG, "HCar connection suspended: " + cause);
        }
    };

    private OnConnectionFailedListener connectionFailedListener = result -> {
        Log.e(TAG, "HCar connect failed: " + result.getErrorCode() + " " + 
                   result.getErrorMessage());
    };

    // ========== 1. 发现车机 ==========
    private void discoverCars() {
        hCar.discoverCar(new ConnectionCallback() {
            @Override
            public void onDeviceFound(CarDevice device) {
                Log.i(TAG, "Found car: " + device.getDeviceName() + 
                           " addr=" + device.getAddress());
                // 展示给用户，点击后连接
                showDeviceInUI(device);
            }
        });
    }

    // ========== 2. 连接车机 ==========
    public void connectToCar(CarDevice device) {
        hCar.connect(device, new ConnectionCallback() {
            @Override
            public void onConnected() {
                Log.i(TAG, "Connected to car");
                // 注册车控回调
                registerCarControl();
                // 开始投屏
                startProjection();
            }

            @Override
            public void onError(int errorCode) {
                Log.e(TAG, "Connect failed: " + errorCode);
            }
        });
    }

    // ========== 3. 注册车控回调（接收车机状态） ==========
    private void registerCarControl() {
        hCar.registerCarControlCallback(new CarControlCallback() {
            @Override
            public void onVehicleDataUpdate(VehicleData data) {
                Log.i(TAG, "Vehicle data: speed=" + data.getSpeed() + 
                           " fuel=" + data.getFuelLevel());
            }

            @Override
            public void onError(int errorCode) {
                Log.e(TAG, "Car control error: " + errorCode);
            }
        });
    }

    // ========== 4. 发起车控指令（带签名） ==========
    public void sendControlCommand(String action, JSONObject params) {
        try {
            // 1. 准备签名数据
            String payload = action + ":" + params.toString();
            
            // 2. 用手机端私钥签名（需在 TEE/Keystore 中）
            String signature = SignatureUtil.signWithDeviceKey(payload);
            
            // 3. 构造控制指令
            ControlCommand command = new ControlCommand.Builder()
                .setAction(action)
                .setParams(params.toString())
                .setSignature(signature)
                .setCertHash(KeyStoreManager.getDeviceCertHash())
                .build();

            // 4. 发送
            hCar.sendControlCommand(command, new CarControlCallback() {
                @Override
                public void onResult(boolean success, String result) {
                    Log.i(TAG, "Control result: " + success + " " + result);
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Send control command failed", e);
        }
    }

    // ========== 5. 投屏 ==========
    private void startProjection() {
        hCar.startProjection(new ProjectionCallback() {
            @Override
            public void onProjectionStarted() {
                Log.i(TAG, "Projection started");
            }

            @Override
            public void onProjectionStopped() {
                Log.i(TAG, "Projection stopped");
            }
        });
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        if (hCar != null) hCar.disconnect();
        if (client != null) client.disconnect();
        super.onDestroy();
    }
}
```

---

### 五、联调流程（关键步骤）

#### 阶段 1：开发自测（Dev 环境）
```
1. 申请沙盒（Sandbox）测试资格
   - 华为 HiCar Open Platform → 沙盒环境
   - 获得沙盒 appId（区别于生产）

2. 车机端 HiCar APP 预装到测试车机
   - 通过 ADB 安装或 OEM 工厂模式安装
   - 启用 HiCar 调试模式：*#*#284#*#* （HiCar 调试码）

3. 手机端开发联调
   - 开发者手机（华为/荣耀）开启 USB 调试
   - 数据线连接车机 USB 口
   - 车机 HiCar 应自动识别手机端 HiCar Service
```

#### 阶段 2：协议握手
```
手机端请求 ───► 车机端响应
   │                 │
   │ ① 蓝牙发现      │
   ├────────────────►│
   │ ② 配对          │
   │◄────────────────┤
   │ ③ USB/WiFi 建立 │
   ├────────────────►│
   │ ④ HiCar 协议握手│
   │   (TLS 双向认证)│
   │◄────────────────┤
   │ ⑤ 能力协商      │
   │   (capabilities)│
   │◄────────────────┤
   │ ⑥ 会话建立 OK   │
```

#### 阶段 3：日志调试

```bash
# 车机端查看 HiCar 日志
adb shell hilog -x | grep -i hicar

# 手机端查看 HiCar 日志
adb logcat -s HiCar:V HiCarService:V HCar:V

# 抓 HiCar 协议包（USB）
adb shell tcpdump -i usb0 -w /sdcard/hicar.pcap

# 在 Wireshark 解析 HiCar 私有协议
# (需 HiCar Wireshark 插件，OEM 提供)
```

#### 阶段 4：常见联调问题

| 问题           | 排查方向                                                    |
| -------------- | ----------------------------------------------------------- |
| 手机搜不到车机 | 蓝牙/WiFi 权限；HiCar Service 是否启动；车机 HiCar 是否启用 |
| 连接上立即断开 | 证书不匹配；时间不同步；车机端 HiCar 版本不兼容             |
| 投屏黑屏       | 分辨率不匹配；编码参数协商失败；H.265 硬解不支持            |
| 车控指令无响应 | 签名验证失败；TBOX 未连接；CAN 总线未配置                   |
| 频繁断连       | WiFi 信号弱；USB 供电不足；车机蓝牙被其他应用占用           |

---

### 六、HiCar 协议握手示例（关键安全点）

```typescript
// 车机端：TLS 双向认证握手
async function hicarTlsHandshake(phoneSocket: net.Socket): Promise<void> {
  // 1. 加载车机端证书（HUKS 中存储）
  const carCert = await huks.exportCert('car_device_cert');
  const carKeyHandle = await huks.getKey('car_device_key');
  
  // 2. 启动 mTLS
  const sslContext = tls.createSecureContext({
    cert: carCert,
    key: carKeyHandle,           // HUKS 引用
    ca: loadPhoneCARoots(),      // 信任的手机 CA
    requireClientCert: true,     // 强制验证手机
    protocols: ['TLSv1.3'],
    ciphers: ['TLS_AES_256_GCM_SHA384']
  });
  
  // 3. 完成握手
  const secureSocket = await tls.accept(sslContext, phoneSocket);
  
  // 4. 校验手机证书链 → OCSP 检查
  const phoneCert = secureSocket.getPeerCertificate();
  const certStatus = await ocspCheck(phoneCert);
  if (certStatus !== 'good') {
    secureSocket.destroy();
    throw new Error('Phone cert revoked or unknown');
  }
  
  Logger.info('HiCar TLS handshake done');
}
```

---

### 七、HiCar 应用上架流程

```
1. 集成测试通过
        ↓
2. 提交 HiCar 兼容性认证（华为提供测试用例）
        ↓
3. 提交安全合规材料（ISO 21434 / CC EAL 认证）
        ↓
4. HiCar Open Platform 提交应用
        ↓
5. 华为审核（功能 / 性能 / 安全）
        ↓
6. HiCar 应用商店上架
        ↓
7. 持续 OTA 更新维护
```

---

### 八、调试工具清单

| 工具                                     | 用途           |
| ---------------------------------------- | -------------- |
| **HiCar Test Assistant**（华为提供 App） | 测试车机功能   |
| **HiCar Debug Bridge** (`hicadb`)        | 调试协议       |
| **Hilink Device Tools**                  | 设备模拟       |
| **Wireshark + HiCar 插件**               | 抓包分析       |
| **DevEco Studio**                        | 鸿蒙车机端 IDE |
| **Android Studio + HMS Plugin**          | 手机端 IDE     |
| **Vehicle Spy / CANoe**                  | 整车联调       |

---

## ❓ 下一步深入方向

请告诉我你接下来要重点攻克哪个方向，我可以继续展开：

1. **车机端 HUKS 集成 + 证书签发**（CSR/CA/OCSP 完整流程）？
2. **车机端 TBOX-PKI 接口**（证书申请、轮换、状态查询）？
3. **TEE 远程证明**（车机端 Attestation 协议）？
4. **国密 SM2/SM4 在车机端的应用**？
5. **HiCar 协议抓包分析**（实战 Wireshark）？
6. **HiCar 与座舱 / 仪表 / HUD 多屏协同**？
7. **ISO 21434 车规安全合规**如何落到代码层面？

告诉我你的下一步重点，我接着展开。