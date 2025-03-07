## 1\. 多用户多屏（CarService）

google在如下提交引入了`CarOccupantZoneManager`和`CarOccupantZoneService`：

```
commit bb877e25c080aeabaae10f12e2f725dae0a65f90
Author: Keun young Park <keunyoung@google.com>
Date:   Fri Aug 2 10:38:01 2019 -0700

    Add CarOccupantZoneManager API and service
    
    - Added minimum skeleton to present available zones and return
      Display related information.
    - Added unit test for Service and Manager
    - Added two overlay configs to configure zones and display mapping for that.
    
    TODO:
     - Add secondary display user handling
     - Add manual tests / apps to use this
    
    Bug: 138860751
    Test: Run added tests
          atest com.android.car.CarOccupantZoneServiceTest
          atest android.car.apitest.VehicleSeatTest
    Change-Id: I7867ea6b554695b958f5ed527a0f32cdd281c2c2

```

从注释看这两个类用来是用来处理`zones`和`Display`的映射关系，`zones`指的是车内的座位区域，一般分为司机和乘客，乘客也会根据位置细分前后排，左右，中间，从这个两个类的引入看起来google已经想到了车内每一个座位可能对应一块屏幕。

`CarOccupantZoneManager`中定义了`屏幕`和`车内乘员`的类型：

```
/**
 * API to get information on displays and users in the car.
 */
public class CarOccupantZoneManager extends CarManagerBase {
       
     /** Display type is not known. In some system, some displays may be just public display without
     *  any additional information and such displays will be treated as unknown.
     */
    public static final int DISPLAY_TYPE_UNKNOWN = 0;

    /** Main display users are interacting with. UI for the user will be launched to this display by
     *  default. {@link Display#DEFAULT_DISPLAY} will be always have this type. But there can be
     *  multiple of this type as each passenger can have their own main display.
     */
    public static final int DISPLAY_TYPE_MAIN = 1;

    /** Instrument cluster display. This may exist only for driver. */
    public static final int DISPLAY_TYPE_INSTRUMENT_CLUSTER = 2;

    /** Head Up Display. This may exist only for driver. */
    public static final int DISPLAY_TYPE_HUD = 3;

    /** Dedicated display for showing IME for {@link #DISPLAY_TYPE_MAIN} */
    public static final int DISPLAY_TYPE_INPUT = 4;
    
    .......
            
       /** @hide */
    public static final int OCCUPANT_TYPE_INVALID = -1;

    /** Represents driver. There can be only one driver for the system. */
    public static final int OCCUPANT_TYPE_DRIVER = 0;

    /** Represents front passengers who sits in front side of car. Most cars will have only
     *  one passenger of this type but this can be multiple. */
    public static final int OCCUPANT_TYPE_FRONT_PASSENGER = 1;

    /** Represents passengers in rear seats. There can be multiple passengers of this type. */
    public static final int OCCUPANT_TYPE_REAR_PASSENGER = 2;
    
    ......
 
}
```

另外提供了两个配置值来定义`屏幕`和`车类乘员`的详细信息，这两个配置值注释写的比较清楚，还有例子，

可以看到`config_occupant_zones`定义的是`车内乘员`，一条`item`代表一个乘员，包含乘员ID，乘员类型，乘员座位信息。

`config_occupant_display_mapping`定义的是`屏幕`信息，一条`item`代表一块屏幕，包含屏幕端口号（一块物理屏幕有唯一的端口号，如果是虚拟屏幕则需要将`displayPort`改为`displayUniqueId`），屏幕类型，乘员ID。

这两个配置值中定义的`屏幕`和`车内乘员`是有映射关系的，通过配置可以为每一个`车内乘员`对应一块`屏幕`，看起来也可以一个`车内乘员`多块`屏幕`。

从这里我们还可以猜想，一个`车内乘员`会不会就是一个Android中的`User`呢，如果是的话google可能正在实现多用户多屏的功能。

```
<!--
        Lists all occupant (= driver + passenger) zones available in the car.
        Some examples are:
        <item>occupantZoneId=0,occupantType=DRIVER,seatRow=1,seatSide=driver</item>
        <item>occupantZoneId=1,occupantType=FRONT_PASSENGER,seatRow=1,seatSide=oppositeDriver</item>
        <item>occupantZoneId=2,occupantType=REAR_PASSENGER,seatRow=2,seatSide=left</item>
        <item>occupantZoneId=3,occupantType=REAR_PASSENGER,seatRow=2,seatSide=right</item>

        occupantZoneId: Unique unsigned integer id to represent each passenger zone. Each zone
                        should have different id.
        occupantType: Occupant type for the display. Use * part from
                       CarOccupantZoneManager.OCCUPANT_TYPE_* like DRIVER, FRONT_PASSENGER,
                       REAR_PASSENGER and etc.
        seatRow: Integer telling which row the seat is located. Row 1 is for front seats.
        seatSide: left/center/right for known side. Or can use driver/center/oppositeDriver to
                  handle both right-hand driving and left-hand driving in one place.
                  If car's RHD / LHD is not specified, LHD will be assumed and driver side becomes
                  left.
    -->
    <string-array translatable="false" name="config_occupant_zones">
        <item>occupantZoneId=0,occupantType=DRIVER,seatRow=1,seatSide=driver</item>
    </string-array>
    <!--
        Specifies configuration of displays in system telling its usage / type and assigned
        occupant. DEFAULT_DISPLAY, if assigned here, should be always assigned to the DRIVER zone.

        Some examples are:
        <item>displayPort=0,displayType=MAIN,occupantZoneId=0</item>
        <item>displayPort=1,displayType=INSTRUMENT_CLUSTER,occupantZoneId=0</item>
        <item>displayPort=2,displayType=MAIN,occupantZoneId=1</item>
        <item>displayPort=3,displayType=MAIN,occupantZoneId=2</item>
        <item>displayUniqueId=virtual:com.example:MainD,displayType=MAIN,occupantZoneId=3</item>

        NOTE: each item should have displayPort or displayUniqueId, if it has both, displayPort
          will be used.
        displayPort: Unique Port id for the physical display.
        displayUniqueId: Unique Id for the display.
            The unique id of the virtual display will be the form of 'virtual:<PACKAGE>:<ID>'.
        displayType: Display type for the display. Use * part from
                       CarOccupantZoneManager.DISPLAY_TYPE_* like MAIN, INSTRUMENT_CLUSTER and
                       etc.
        occupantZoneId: occupantZoneId specified from config_occupant_zones.

    -->
    <string-array translatable="false" name="config_occupant_display_mapping">
    </string-array>
```

接下来从代码中看看google是如何使用这两个配置值的，`CarService`启动时会解析这两个配置值：

```
    public final class CarOccupantZoneService extends ICarOccupantZone.Stub
        implements CarServiceBase {
        ......
             @Override
    public void init() {
           
        Car car = new Car(mContext, /* service= */null, /* handler= */ null);
        CarInfoManager infoManager = new CarInfoManager(car, CarLocalServices.getService(
                CarPropertyService.class));
         //获取司机的位置，从VehicleHal中读取
        int driverSeat = infoManager.getDriverSeat();
           synchronized (mLock) {
            //司机的位置   
            mDriverSeat = driverSeat;
           //1. 解析config_occupant_zones
            parseOccupantZoneConfigsLocked();
            //2. 解析config_occupant_display_mapping
            parseDisplayConfigsLocked();
             //3. 
            handleActiveDisplaysLocked();
             //4.
            handleAudioZoneChangesLocked();
              //5.
            handleUserChangesLocked();
        }
        }
        ......
}
```

重点来看上述五个方法。

### 1.1 parseOccupantZoneConfigsLocked

```
private void parseOccupantZoneConfigsLocked() {
        final Resources res = mContext.getResources();
        // examples:
        // <item>occupantZoneId=0,occupantType=DRIVER,seatRow=1,seatSide=driver</item>
        // <item>occupantZoneId=1,occupantType=FRONT_PASSENGER,seatRow=1,
        // searSide=oppositeDriver</item>
        //是否是司机
        boolean hasDriver = false;
        //司机的位置
        int driverSeat = getDriverSeat();
        //默认司机位置在左边
        int driverSeatSide = VehicleAreaSeat.SIDE_LEFT; // default LHD : Left Hand Drive
        
        if (driverSeat == VehicleAreaSeat.SEAT_ROW_1_RIGHT) {
            //如果司机位置在第一排右边，则将司机位置改为右边
            driverSeatSide = VehicleAreaSeat.SIDE_RIGHT;
        }
        //OccupantZoneInfo.INVALID_ZONE_ID = -1
        int maxZoneId = OccupantZoneInfo.INVALID_ZONE_ID;  
       //解析config_occupant_zones
        for (String config : res.getStringArray(R.array.config_occupant_zones)) {
            
            //定义四个变量来存储config_occupant_zones中的一个item
            int zoneId = OccupantZoneInfo.INVALID_ZONE_ID;
            int type = CarOccupantZoneManager.OCCUPANT_TYPE_INVALID;
            int seatRow = 0; // invalid row
            int seatSide = VehicleAreaSeat.SIDE_LEFT;
            
            //根据"，"分割item
            String[] entries = config.split(",");
            for (String entry : entries) {
                
                 //根据"="分割item中的每一项数据
                String[] keyValuePair = entry.split("=");
                if (keyValuePair.length != 2) {
                    //不符合规则的item
                    throwFormatErrorInOccupantZones("No key/value pair:" + entry);
                }
                switch (keyValuePair[0]) {
                    case "occupantZoneId":
                        zoneId = Integer.parseInt(keyValuePair[1]);
                        break;
                    case "occupantType":
                        switch (keyValuePair[1]) {
                            case "DRIVER":
                                type = CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER;
                                break;
                            case "FRONT_PASSENGER":
                                type = CarOccupantZoneManager.OCCUPANT_TYPE_FRONT_PASSENGER;
                                break;
                            case "REAR_PASSENGER":
                                type = CarOccupantZoneManager.OCCUPANT_TYPE_REAR_PASSENGER;
                                break;
                            default:
                                throwFormatErrorInOccupantZones("Unrecognized type:" + entry);
                                break;
                        }
                        break;
                    case "seatRow":
                        seatRow = Integer.parseInt(keyValuePair[1]);
                        break;
                    case "seatSide":
                        switch (keyValuePair[1]) {
                            case "driver":
                                seatSide = driverSeatSide;
                                break;
                            case "oppositeDriver":
                                seatSide = -driverSeatSide;
                                break;
                            case "left":
                                seatSide = VehicleAreaSeat.SIDE_LEFT;
                                break;
                            case "center":
                                seatSide = VehicleAreaSeat.SIDE_CENTER;
                                break;
                            case "right":
                                seatSide = VehicleAreaSeat.SIDE_RIGHT;
                                break;
                            default:
                                throwFormatErrorInOccupantZones("Unregognized seatSide:" + entry);
                                break;

                        }
                        break;
                    default:
                        throwFormatErrorInOccupantZones("Unrecognized key:" + entry);
                        break;
                }
            }
            /**
               上述switch case将config_occupant_zones中item的每一项数据保存了下来
             */
            
            
            if (zoneId == OccupantZoneInfo.INVALID_ZONE_ID) {
                throwFormatErrorInOccupantZones("Missing zone id:" + config);
            }
            if (zoneId > maxZoneId) {
                maxZoneId = zoneId;
            }
            if (type == CarOccupantZoneManager.OCCUPANT_TYPE_INVALID) {
                throwFormatErrorInOccupantZones("Missing type:" + config);
            }
            if (type == CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER) {
                if (hasDriver) {
                    throwFormatErrorInOccupantZones("Multiple driver:" + config);
                } else {
                    hasDriver = true;
                    mDriverZoneId = zoneId;
                }
            }
            //这里是将seatRow和seatSide经过运算变为一个值，seatRow和seatSide共同来确定车内乘员的位置
            int seat = VehicleAreaSeat.fromRowAndSide(seatRow, seatSide);
            if (seat == VehicleAreaSeat.SEAT_UNKNOWN) {
                throwFormatErrorInOccupantZones("Invalid seat:" + config);
            }
            //OccupantZoneInfo用来描述车内乘员详细信息
            OccupantZoneInfo info = new OccupantZoneInfo(zoneId, type, seat);
            
            if (mOccupantsConfig.contains(zoneId)) {
                throwFormatErrorInOccupantZones("Duplicate zone id:" + config);
            }
            //车内所有乘员都保存到mOccupantsConfig中，以乘员ID为唯一Key
            mOccupantsConfig.put(zoneId, info);
        }
        if (!hasDriver) {
            //config_occupant_zones中没有配置司机的情况
            maxZoneId++;
            mDriverZoneId = maxZoneId;
            Slogf.w(TAG, "No driver zone, add one:%d", mDriverZoneId);
            OccupantZoneInfo info = new OccupantZoneInfo(mDriverZoneId,
                    CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER, getDriverSeat());
            mOccupantsConfig.put(mDriverZoneId, info);
        }
    }
```

`config_occupant_zones`中定义的`item`在代码中被转换为一个`OccupantZoneInfo`，`OccupantZoneInfo`被保存在`mOccupantsConfig`中，通过乘员ID来获取。

### 1.2 parseDisplayConfigsLocked

```
private void parseDisplayConfigsLocked() {
        final Resources res = mContext.getResources();
        // examples:
        // <item>displayPort=0,displayType=MAIN,occupantZoneId=0</item>
        // <item>displayPort=1,displayType=INSTRUMENT_CLUSTER,occupantZoneId=0</item>
    
        //解析config_occupant_display_mapping
        for (String config : res.getStringArray(R.array.config_occupant_display_mapping)) {
            
            //定义四个变量来存储config_occupant_display_mapping中的一个item
            int port = INVALID_PORT;
            String uniqueId = null;
            int type = CarOccupantZoneManager.DISPLAY_TYPE_UNKNOWN;
            int zoneId = OccupantZoneInfo.INVALID_ZONE_ID;
            String[] entries = config.split(",");
            //根据"，"分割
            for (String entry : entries) {
                //根据"="分割item中的每一项数据
                String[] keyValuePair = entry.split("=");
                if (keyValuePair.length != 2) {
                    throwFormatErrorInDisplayMapping("No key/value pair:" + entry);
                }
                switch (keyValuePair[0]) {
                    case "displayPort":
                        port = Integer.parseInt(keyValuePair[1]);
                        break;
                    case "displayUniqueId":
                        uniqueId = keyValuePair[1];
                        break;
                    case "displayType":
                        switch (keyValuePair[1]) {
                            case "MAIN":
                                type = CarOccupantZoneManager.DISPLAY_TYPE_MAIN;
                                break;
                            case "INSTRUMENT_CLUSTER":
                                type = CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER;
                                break;
                            case "HUD":
                                type = CarOccupantZoneManager.DISPLAY_TYPE_HUD;
                                break;
                            case "INPUT":
                                type = CarOccupantZoneManager.DISPLAY_TYPE_INPUT;
                                break;
                            case "AUXILIARY":
                                type = CarOccupantZoneManager.DISPLAY_TYPE_AUXILIARY;
                                break;
                            default:
                                throwFormatErrorInDisplayMapping(
                                        "Unrecognized display type:" + entry);
                                break;
                        }
                        break;
                    case "occupantZoneId":
                        zoneId = Integer.parseInt(keyValuePair[1]);
                        break;
                    default:
                        throwFormatErrorInDisplayMapping("Unrecognized key:" + entry);
                        break;

                }
            }
            // Now check validity
            if (port == INVALID_PORT && uniqueId == null) {
                //如果port = -1，并且没有定义displayUniqueId则说明是无效的item
                throwFormatErrorInDisplayMapping(
                        "Missing or invalid displayPort and displayUniqueId:" + config);
            }

            if (type == CarOccupantZoneManager.DISPLAY_TYPE_UNKNOWN) {
                //无效的屏幕类型
                throwFormatErrorInDisplayMapping("Missing or invalid displayType:" + config);
            }
            if (zoneId == OccupantZoneInfo.INVALID_ZONE_ID) {
                //无效的车内乘员ID
                throwFormatErrorInDisplayMapping("Missing or invalid occupantZoneId:" + config);
            }
            if (!mOccupantsConfig.contains(zoneId)) {
                //无效的车内乘员ID，config_occupant_display_mapping中定义的乘员ID必须包含在config_occupant_zones中
                throwFormatErrorInDisplayMapping(
                        "Missing or invalid occupantZoneId:" + config);
            }
            //DisplayConfig用来描述屏幕类型与车内乘员ID的映射关系
            DisplayConfig displayConfig = new DisplayConfig(type, zoneId);
            
            if (port != INVALID_PORT) {//物理屏幕
                
                if (mDisplayPortConfigs.contains(port)) {
                    //重复定义的物理屏幕端口
                    throwFormatErrorInDisplayMapping("Duplicate displayPort:" + config);
                }
                //DisplayConfig保存到mDisplayPortConfigs中，以物理屏幕端口为Key
                mDisplayPortConfigs.put(port, displayConfig);
                
            } else {//虚拟屏幕
                
                if (mDisplayUniqueIdConfigs.containsKey(uniqueId)) {
                    throwFormatErrorInDisplayMapping("Duplicate displayUniqueId:" + config);
                }
                //虚拟屏幕DisplayConfig保存在mDisplayUniqueIdConfigs中
                mDisplayUniqueIdConfigs.put(uniqueId, displayConfig);
            }
        }
    }
```

`config_occupant_display_mapping`中可以定义物理屏幕和虚拟屏幕，`DisplayConfig`用来描述屏幕类型与车内乘员的映射关系，物理屏幕保存在`mDisplayPortConfigs`中，以物理屏幕端口port为Key，虚拟屏幕保存在`mDisplayUniqueIdConfigs`中，以`displayUniqueId`为Key。

### 1.3 handleActiveDisplaysLocked

```
private void handleActiveDisplaysLocked() {
        mActiveOccupantConfigs.clear();
        boolean hasDefaultDisplayConfig = false;
        //遍历当前系统存在的所有屏幕
        for (Display display : mDisplayManager.getDisplays()) {
            //findDisplayConfigForDisplayLocked的作用是去mDisplayPortConfig或者mDisplayUniqueIdConfigs中找屏幕对应的DisplayConfig，
            //物理屏幕或者虚拟屏幕
            DisplayConfig displayConfig = findDisplayConfigForDisplayLocked(display);
            if (displayConfig == null) {
                //config_occupant_display_mapping配置写错了
                Slogf.w(TAG, "Display id:%d does not have configurationions",
                        display.getDisplayId());
                continue;
            }
            if (display.getDisplayId() == Display.DEFAULT_DISPLAY) {
                if (displayConfig.occupantZoneId != mDriverZoneId) {
                    //这里就规定了默认屏幕的持有者必须是司机
                    throw new IllegalStateException(
                            "Default display should be only assigned to driver zone");
                }
                hasDefaultDisplayConfig = true;
            }
            //这个方法的目的是为车内乘员分配屏幕
            addDisplayInfoToOccupantZoneLocked(displayConfig.occupantZoneId,
                    new DisplayInfo(display, displayConfig.displayType));
        }
        if (!hasDefaultDisplayConfig) {
            // 这个分支说明默认屏幕没有配置到config_occupant_display_mapping或者配置写错了
            //这种情况还是需要将默认屏幕分配给司机
            Slogf.w(TAG, "Default display not assigned, will assign to driver zone");
            addDisplayInfoToOccupantZoneLocked(mDriverZoneId, new DisplayInfo(
                    mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY),
                    CarOccupantZoneManager.DISPLAY_TYPE_MAIN));
        }
    }
```

#### 1.3.1 addDisplayInfoToOccupantZoneLocked

```
  private void addDisplayInfoToOccupantZoneLocked(int zoneId, DisplayInfo info) {
       //mActiveOccupantConfigs用来存储车内乘员ID和OccupantConfig的映射关系
      //OccupantConfig对象中又保存了userId和DisplayInfo以及音频区域ID的对应关系，音频这块还没看，但猜想和屏幕与乘员映射差不多
        OccupantConfig occupantConfig = mActiveOccupantConfigs.get(zoneId);
        if (occupantConfig == null) {
            occupantConfig = new OccupantConfig();
            //建立车内乘员ID与OccupantConfig的映射关系
            mActiveOccupantConfigs.put(zoneId, occupantConfig);
        }
      //保存此乘员对应的屏幕，OccupantConfig中的displayInfos变量是一个集合，说明一个乘员可对应多个屏幕
        occupantConfig.displayInfos.add(info);
    }
```

`OccupantConfig`这个类定义了三个变量，从这三个变量其实可以看出一个用户可以对应一个或多个屏幕，同时对应音频，音频这块怎么对应的暂时还不清楚，其实这种对应关系是好理解的，因为每个用户都应该有自己的[音视频](https://so.csdn.net/so/search?q=%E9%9F%B3%E8%A7%86%E9%A2%91&spm=1001.2101.3001.7020)区域，而`mActiveOccupantConfigs`又将`车内乘员`和`OccupantConfig`对应了起来，现在基本可以确定一个车内乘员其实就可以对应一个Android系统中的`User`。

```
    static class OccupantConfig {
        public int userId = UserHandle.USER_NULL;
        public final ArrayList<DisplayInfo> displayInfos = new ArrayList<>();
        public int audioZoneId = CarAudioManager.INVALID_AUDIO_ZONE;
        .....
        }
```

上面代码还没看到`OccupantConfig`的`userId`怎么赋值的。

init中的步骤4`handleAudioZoneChangesLocked`是处理音频这块的，暂时跳过。

### 1.4 handleUserChangesLocked

```
    private void handleUserChangesLocked() {
        //司机用户为当前用户
        int driverUserId = getCurrentUser();
       //是否为多屏分配用户
        if (mEnableProfileUserAssignmentForMultiDisplay) {
            updateEnabledProfilesLocked(driverUserId);
        }
       
        for (int i = 0; i < mActiveOccupantConfigs.size(); ++i) {
            int zoneId = mActiveOccupantConfigs.keyAt(i);
            OccupantConfig config = mActiveOccupantConfigs.valueAt(i);
            //1.4.1可知mProfileUsers中存储了当前用户下的所有profile user
            if (mProfileUsers.contains(config.userId)) {
                Slogf.i(TAG, "Profile user:%d already assigned for occupant zone:%d",
                        config.userId, zoneId);
            } else {
                //这里将所有屏幕对应的userId都赋值为当前用户
                //意思CarService初始化情况下，所有屏幕，所有车内乘员都是同一个用户
                config.userId = driverUserId;
            }
        }
    }
```

`mEnableProfileUserAssignmentForMultiDisplay`定义在`config.xml`中：

```
 <!-- Enable profile user assignment per each CarOccupantZone for per display android user
         assignments. This feature is still experimental. -->
    <bool name="enableProfileUserAssignmentForMultiDisplay" translatable="false">false</bool>
```

从注释大概可以看出，这个值用来控制是否为每一个`Display`分配`profile user`，重点最后一句，此功能尚在实验阶段，并且这个功能默认为false，所以至少在Android12上google并没有实现多用户多屏的功能。

#### 1.4.1 updateEnabledProfilesLocked

```
private void updateEnabledProfilesLocked(int userId) {
        mProfileUsers.clear();
       //获取指定userId之下的所有profile user
        List<UserInfo> profileUsers = mUserManager.getEnabledProfiles(userId);
        for (UserInfo userInfo : profileUsers) {
            if (userInfo.id != userId) {
                //保存在mProfileUsers中
                mProfileUsers.add(userInfo.id);
            }
        }
    }
```

`CarOccupantZoneService`初始化的几个重要方法看完了，虽然到最后所有`车内乘员`都根据配置有自己的对应`屏幕`，但默认情况下，所有`车内乘员`依然使用的是当前用户。

### 1.5 为车内乘员分配用户

`CarOccupantZoneService`初始化的几个重要方法看完了，虽然到最后所有`车内乘员`都根据配置有自己的对应`屏幕`，但默认情况下，所有`车内乘员`依然使用的是当前主用户（司机用户）。

接着来看看用户真正的创建与分配：

### 1.5.1 乘客用户的创建

CarUserService:

```
private void onUserSwitching(@UserIdInt int fromUserId, @UserIdInt int toUserId) {

        ......
          //这里有两个判断条件，第一个是：config.xml中的配置开关enablePassengerSupport，Android12上默认关闭
           //第二个是：config_occupant_display_mapping中为乘客配置的屏幕是否是有效的
        if (mEnablePassengerSupport && isPassengerDisplayAvailable()) {
            //当两个条件满足则会创建乘客用户
            
            //创建乘客用户
            setupPassengerUser();
            //分配乘客用户
            startFirstPassenger(toUserId);
            
        }
        t.traceEnd();
    }
```

### 1.5.2 setupPassengerUser

```
private void setupPassengerUser() {
        int currentUser = ActivityManager.getCurrentUser();
       //获取主用户下的子用户数量
        int profileCount = getNumberOfManagedProfiles(currentUser);
        if (profileCount > 0) {
            Slog.w(TAG, "max profile of user" + currentUser
                    + " is exceeded: current profile count is " + profileCount);
            return;
        }

        // 创建乘客用户
        UserInfo passenger = createPassenger("Passenger", currentUser);
        if (passenger == null) {
            // Couldn't create user, most likely because there are too many.
            Slog.w(TAG, "cannot create a passenger user");
            return;
        }
    }
```

### 1.5.3 createPassenger

```
public UserInfo createPassenger(@NonNull String name, @UserIdInt int driverId) {
        ......
            //省略权限判断，司机有效性判断
            
            //创建子用户
        UserInfo user = mUserManager.createProfileForUser(name,
                UserManager.USER_TYPE_PROFILE_MANAGED, /* flags */ 0, driverId);
       .......
        return user;
    }
```

### 1.5.4 startFirstPassenger

```
private boolean startFirstPassenger(@UserIdInt int driverId) {
        //获取第一个有效的乘客zoneId，这里可知，目前只支持一个司机下一个乘客子用户
        int zoneId = getAvailablePassengerZone();
        if (zoneId == OccupantZoneInfo.INVALID_ZONE_ID) {
            Slog.w(TAG, "passenger occupant zone is not found");
            return false;
        }
    //获取给定driverId下的所有乘客子用户
        List<UserInfo> passengers = getPassengers(driverId);
        if (passengers.size() < 1) {
            Slog.w(TAG, "passenger is not found");
            return false;
        }
      //拿到第一个乘客子用户
        int passengerId = passengers.get(0).id;
    //分配乘客子用户
        if (!startPassenger(passengerId, zoneId)) {
            Slog.w(TAG, "cannot start passenger " + passengerId);
            return false;
        }
        return true;
    }
```

上述代码可知就算一个司机用户下有多个乘客子用户，但仍然只支持第一个子用户的分配

### 1.5.5 startPassenger

```
@Override
    public boolean startPassenger(@UserIdInt int passengerId, int zoneId) {
        checkManageUsersPermission("startPassenger");
        synchronized (mLockUser) {
            try {
                //将此乘客子用户启动
                if (!mAm.startUserInBackgroundWithListener(passengerId, null)) {
                    Slog.w(TAG, "could not start");
                    return false;
                }
            } catch (RemoteException e) {
                Slog.w(TAG, "error while starting passenger", e);
                return false;
            }
            //将乘客子用户ID分配给车内乘员区域
            if (!assignUserToOccupantZone(passengerId, zoneId)) {
                Slog.w(TAG, "could not assign passenger to zone");
                return false;
            }
        }
        ......
        return true;
    }
```

`assignUserToOccupantZone`方法实现在`CarOccupantZoneService`中，

### 1.5.6 assignUserToOccupantZone

```
 @Override
            public boolean assignUserToOccupantZone(@UserIdInt int userId, int zoneId) {
                // Check if the user is already assigned to the other zone.
                synchronized (mLock) {
                    //遍历所有车内乘员
                    for (int i = 0; i < mActiveOccupantConfigs.size(); ++i) {
                        OccupantConfig config = mActiveOccupantConfigs.valueAt(i);
                        //不允许一个用户分配给两个zone
                        if (config.userId == userId && zoneId != mActiveOccupantConfigs.keyAt(i)) {
                            Slogf.w(TAG, "cannot assign user to two different zone simultaneously");
                            return false;
                        }
                    }
                    OccupantConfig zoneConfig = mActiveOccupantConfigs.get(zoneId);
                    if (zoneConfig == null) {
                        Slogf.w(TAG, "cannot find the zone(%d)", zoneId);
                        return false;
                    }
                    if (zoneConfig.userId != UserHandle.USER_NULL && zoneConfig.userId != userId && zoneConfig.userId != getDriverUserId() && zoneConfig.userId != 0) {
                        return false;
                    }
                    if(userId == UserHandle.USER_NULL){
                          return false;
                    }
                    zoneConfig.userId = userId;
                    return true;
                }
            }
```

用户分配完成之后就完全建立了屏幕，车内区域，用户的一一映射关系，通过dump看到结果如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3380b6d5231c338e119af542939fc6b7.png)

用户和屏幕的映射会传到Framework，真正将不同用户分到不同屏幕还是Framework来做的，从目前Android12的调研来看能够实现：**指定userId启动的Activity会启动到和userId所对应的屏幕上**，这点已经基本实现了多用户多屏的雏形。

如下是指定userId启动的clock：**adb shell am start --user 11 com.android.deskclock/.DeskClock**

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/fde012981ba57aa40f9c255c4ee9606e.png)

只需要指定`userId`，此应用就启动到了对应的`display 2`上，进程也属于`user 11`：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/743e77fb1ba04a826c2908ca50b19bdb.png)

## 2\. 多用户多屏（Framework）

### 2.1 system\_server绑定CarService

`system_server`进程启动阶段会和`CarService`建立双向通信，通过绑定`CarService`的方式：

```
public class CarServiceHelperService extends SystemService
        implements Dumpable, DevicePolicySafetyChecker {
        ......
        @Override
    public void onStart() {
            ...
        Intent intent = new Intent();
        intent.setPackage("com.android.car");
        intent.setAction(CAR_SERVICE_INTERFACE);
             //绑定CarService
        if (!mContext.bindServiceAsUser(intent, mCarServiceConnection, Context.BIND_AUTO_CREATE,
                mHandler, UserHandle.SYSTEM)) {
            Slogf.wtf(TAG, "cannot start car service");
        }
                    ...
    }
        ......
 }
```

```
private final ServiceConnection mCarServiceConnection = new ServiceConnection() {
    @Override
    public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
        
        handleCarServiceConnection(iBinder);
    
    }

    @Override
    public void onServiceDisconnected(ComponentName componentName) {
        handleCarServiceCrash();
    }
};
```

绑定`CarService`拿到其`Binder`对象之后会将`CarServiceHelperService`内部的一个`Binder`，传给`CarService`，自此`CarService`可访问`system_server`，

```
private void sendSetSystemServerConnectionsCall() {
        Parcel data = Parcel.obtain();
        data.writeInterfaceToken(CAR_SERVICE_INTERFACE);
       //重点，将mHelper.asBinder()传给了CarService
        data.writeStrongBinder(mHelper.asBinder());
        data.writeStrongBinder(mCarServiceConnectedCallback.asBinder());
        IBinder binder;
        synchronized (mLock) {
            //mCarServiceBinder就是绑定<code>CarService</code>之后返回的binder
            binder = mCarServiceBinder;
        }
        int code = IBinder.FIRST_CALL_TRANSACTION;
        try {
          
            // oneway void setSystemServerConnections(in IBinder helper, in IBinder receiver) = 0;
            binder.transact(code, data, null, Binder.FLAG_ONEWAY);
            ...
        } 
         ....
    }
```

`mHelper`为`CarService`提供了如下访问接口：

```
private final ICarServiceHelperImpl mHelper = new ICarServiceHelperImpl();

private class ICarServiceHelperImpl extends ICarServiceHelper.Stub {
     
       ......
        @Override
        public void setDisplayAllowlistForUser(@UserIdInt int userId, int[] displayIds) {
            mCarLaunchParamsModifier.setDisplayAllowListForUser(userId, displayIds);
        }

        @Override
        public void setPassengerDisplays(int[] displayIdsForPassenger) {
            mCarLaunchParamsModifier.setPassengerDisplays(displayIdsForPassenger);
        }
         ......
    }

```

`ICarServiceHelperImpl`提供了好几个方法，重点关注上面两个，`CarService`会通过上述方法将用户和屏幕的映射关系传到Framework。

### 2.2 CarService数据同步到Framework

`CarOccupantZoneService`中有个方法`doSyncWithCarServiceHelper`，这个方法的作用是将用户或者屏幕的变化情况同步给Framework，例如新增或减少一对屏幕和用户映射等：

```
private void doSyncWithCarServiceHelper(@Nullable ICarServiceHelper helper,
            boolean updateDisplay, boolean updateUser, boolean updateConfig) {
        int[] passengerDisplays = null;
        ArrayMap<Integer, IntArray> allowlists = null;
    //参数helper就是Framework传过来的ICarServiceHelperImpl
        ICarServiceHelper helperToUse = helper;
        synchronized (mLock) {
            if (helper == null) {
                if (mICarServiceHelper == null) { // helper not set yet.
                    return;
                }
                helperToUse = mICarServiceHelper;
            } else {
                mICarServiceHelper = helper;
            }
            //是否更新了屏幕
            if (updateDisplay) {
                passengerDisplays = getAllActivePassengerDisplaysLocked();
            }
            //是否更新了用户
            if (updateUser) {
                allowlists = createDisplayAllowlistsLocked();
            }
        }
        //是否更新了屏幕
        if (updateDisplay) {
            //更新数据到Framework
            updatePassengerDisplays(helperToUse, passengerDisplays);
        }
        //是否更新了用户
        if (updateUser) {
            //更新数据到Framework
            updateUserAssignmentForDisplays(helperToUse, allowlists);
        }
        //是否更新了config_sourcePreferredComponents配置
        if (updateConfig) {
            Resources res = mContext.getResources();
            String[] components = res.getStringArray(R.array.config_sourcePreferredComponents);
             //更新数据到Framework
            updateSourcePreferredComponents(helperToUse, components);
        }
    }
```

代码逻辑很清晰，当用户，屏幕，或者`config_sourcePreferredComponents`任意一个发生了变化都会通知Framework，

屏幕更新：

```
private int[] getAllActivePassengerDisplaysLocked() {
    IntArray displays = new IntArray();
    //mActiveOccupantConfigs这个集合前面讲过，它里面存储了车内乘员，user，display的对应关系
    for (int j = 0; j < mActiveOccupantConfigs.size(); ++j) {
        int zoneId = mActiveOccupantConfigs.keyAt(j);
        if (zoneId == mDriverZoneId) {
            //跳过司机
            continue;
        }
        OccupantConfig config = mActiveOccupantConfigs.valueAt(j);
        for (int i = 0; i < config.displayInfos.size(); i++) {
            //将所有除司机以外的屏幕全部加到displays中，一个乘客用户是可能对应多个屏幕的
            displays.add(config.displayInfos.get(i).display.getDisplayId());
        }
    }
    return displays.toArray();
}
```

用户更新：

```
private ArrayMap<Integer, IntArray> createDisplayAllowlistsLocked() {
        ArrayMap<Integer, IntArray> allowlists = new ArrayMap<>();
        for (int j = 0; j < mActiveOccupantConfigs.size(); ++j) {
            int zoneId = mActiveOccupantConfigs.keyAt(j);
            if (zoneId == mDriverZoneId) {
                //跳过司机
                continue;
            }
            OccupantConfig config = mActiveOccupantConfigs.valueAt(j);
            if (config.displayInfos.isEmpty()) {
                continue;
            }
            // user like driver can have multiple zones assigned, so add them all.
            IntArray displays = allowlists.get(config.userId);
            if (displays == null) {
                displays = new IntArray();
                //一个乘客可能对应多个屏幕，displays是一个集合
                allowlists.put(config.userId, displays);
            }
            for (int i = 0; i < config.displayInfos.size(); i++) {
                //一个乘客用户对应的所有屏幕
                displays.add(config.displayInfos.get(i).display.getDisplayId());
            }
        }
        return allowlists;
    }
```

`config_sourcePreferredComponents`这个config暂时还不清楚作用，屏幕和用户更新之后将数据同步到Framework，

updatePassengerDisplays：

```
 private void updatePassengerDisplays(ICarServiceHelper helper, int[] passengerDisplayIds) {
        if (passengerDisplayIds == null) {
            return;
        }
        try {
            //Binder调用到Framework
            helper.setPassengerDisplays(passengerDisplayIds);
        } catch (RemoteException e) {
            Slogf.e(TAG, "ICarServiceHelper.setPassengerDisplays failed", e);
        }
    }
```

updateUserAssignmentForDisplays：

```
private void updateUserAssignmentForDisplays(ICarServiceHelper helper,
            ArrayMap<Integer, IntArray> allowlists) {
        if (allowlists == null || allowlists.isEmpty()) {
            return;
        }
        try {
            for (int i = 0; i < allowlists.size(); i++) {
                int userId = allowlists.keyAt(i);
               // Binder调用到Framework
                helper.setDisplayAllowlistForUser(userId, allowlists.valueAt(i).toArray());
            }
        } catch (RemoteException e) {
            Slogf.e(TAG, "ICarServiceHelper.setDisplayAllowlistForUser failed", e);
        }
    }
```

Framework接收到用户和屏幕的数据会保存在`CarLaunchParamsModifier`中，这个类是在如下提交中引入的：

```
commit 37b82ce41ababde8891e5c3a311767fae7c0e966
Author: Keun young Park <keunyoung@google.com>
Date:   Fri Aug 9 12:37:00 2019 -0700

    Add car specific LaunchParams modification path
    
    - This allows allowing only specific displays to passnger user id.
    - Car servcie should set policy with
      ICarServiceHelper.setPassengerDisplays() and setDisplayWhitelistForUser().
    - ICarServiceHelper.setPassengerDisplays() should be called per display changes
    - ICarServiceHelper.setDisplayWhitelistForUser() should be called per second
      display user changes
    - The module will handle driver user change by itself.
    
    - TODO: policy should be passed from car service, manual test with policy set
    
    Bug: 139160636
    Test: run the added unit test.
    
    Change-Id: Ic381b471388b084e83ce57fbfa10ee530615453e

diff --git a/src/com/android/server/wm/CarLaunchParamsModifier.java b/src/com/android/server/wm/CarLaunchParamsModifier.java
new file mode 100644
index 0000000..ffeb037
--- /dev/null
+++ b/src/com/android/server/wm/CarLaunchParamsModifier.java
```

从这笔提交的描述了来看，此类的作用是在车机上，为应用启动指定启动参数，主要是屏幕的指定，它允许乘客用户在特定的屏幕，这项策略是由`CarServcie`指定的，通过两个方法来设置：`setPassengerDisplays`和`setDisplayAllowlistForUser`，`CarLaunchParamsModifier`的注释也说这个类是用来为汽车控制Activity启动时屏幕的分配的：

```
/**
 * Class to control the assignment of a display for Car while launching a Activity.
 *
 * <p>This one controls which displays users are allowed to launch.
 * The policy should be passed from car service through
 * {@link com.android.internal.car.ICarServiceHelper} binder interfaces. If no policy is set,
 * this module will not change anything for launch process.</p>
 *
 * <p> The policy can only affect which display passenger users can use. Current user, assumed
 * to be a driver user, is allowed to launch any display always.</p>
 */
public final class CarLaunchParamsModifier implements LaunchParamsController.LaunchParamsModifier {
   
    //存储系统中所有除司机以外的屏幕ID
    private final ArrayList<Integer> mPassengerDisplays = new ArrayList<>();
    //存储屏幕ID和用户ID的映射关系，屏幕ID为Key
    private final SparseIntArray mDisplayToProfileUserMapping = new SparseIntArray();
     //存储屏幕ID和用户ID的映射关系，用户ID为Key
    private final SparseIntArray mDefaultDisplayForProfileUser = new SparseIntArray();
    
......
     public void setPassengerDisplays(int[] displayIdsForPassenger) {
        
        synchronized (mLock) {
            for (int id : displayIdsForPassenger) {
                mPassengerDisplays.remove(Integer.valueOf(id));
            }
            // handle removed displays
            for (int i = 0; i < mPassengerDisplays.size(); i++) {
                int displayId = mPassengerDisplays.get(i);
                updateProfileUserConfigForDisplayRemovalLocked(displayId);
            }
            mPassengerDisplays.clear();
            mPassengerDisplays.ensureCapacity(displayIdsForPassenger.length);
            for (int id : displayIdsForPassenger) {
                //将屏幕ID保存到mPassengerDisplays
                mPassengerDisplays.add(id);
            }
        }
    }
    .......
      public void setDisplayAllowListForUser(int userId, int[] displayIds) {
       
        synchronized (mLock) {
            for (int displayId : displayIds) {
                if (!mPassengerDisplays.contains(displayId)) {
                     //如果CarService传过来的屏幕ID不属于乘客，则无效
                    Slog.w(TAG, "setDisplayAllowlistForUser called with display:" + displayId
                            + " not in passenger display list:" + mPassengerDisplays);
                    continue;
                }
                if (userId == mCurrentDriverUser) {
                    //司机用户过滤掉
                    mDisplayToProfileUserMapping.delete(displayId);
                } else {
                    //非司机用户保存下来，通过遍历displayIds，displayIds里面的每一个displayId都映射到同一个userId
                    mDisplayToProfileUserMapping.put(displayId, userId);
                }
                // now the display cannot be a default display for other user
                int i = mDefaultDisplayForProfileUser.indexOfValue(displayId);
                if (i >= 0) {
                    mDefaultDisplayForProfileUser.removeAt(i);
                }
            }
            if (displayIds.length > 0) {
                //而mDefaultDisplayForProfileUser中将userId映射到了displayIds中第一块屏幕
                mDefaultDisplayForProfileUser.put(userId, displayIds[0]);
            } else {
                removeUserFromAllowlistsLocked(userId);
            }
        }
    }  
}
```

### 2.3 计算Activity启动的屏幕

用户与屏幕的数据保存好了，接着如果在Car上启动Activity时就会去计算这个Activity应该显示在那个屏幕上，计算方式主要在两个方法中，`sourceDisplayArea`和

`fallbackDisplayArea`：

```
    //计算此次Activity启动所在的TaskDisplayArea
   @Nullable
    private TaskDisplayArea getAlternativeDisplayAreaForPassengerLocked(int userId,
            @NonNull ActivityRecord activityRecord, @Nullable Request request) {
        //优先调用sourceDisplayArea获取，如果sourceDisplayArea返回null再调用fallbackDisplayArea获取
        TaskDisplayArea sourceDisplayArea = sourceDisplayArea(userId, activityRecord, request);

        return sourceDisplayArea != null ? sourceDisplayArea : fallbackDisplayArea(userId);
    }
```

`sourceDisplayArea`目的是计算当前Activity所在的进程上一次显示的屏幕区域：

```
@Nullable
    private TaskDisplayArea sourceDisplayArea(int userId, @NonNull ActivityRecord activityRecord,
            @Nullable Request request) {
        List<WindowProcessController> candidateControllers = candidateControllers(activityRecord,
                request);
       //启动的Activity的进程存在
        for (int i = 0; i < candidateControllers.size(); i++) {
            WindowProcessController controller = candidateControllers.get(i);
            TaskDisplayArea candidate = controller.getTopActivityDisplayArea();
            //获取上次显示的displayId
            int displayId = candidate != null ? candidate.getDisplayId() : Display.INVALID_DISPLAY;
            //根据上次显示的displayId得到对应的userId
            int userForDisplay = mDisplayToProfileUserMapping.get(displayId, UserHandle.USER_NULL);
            //如果此进程上次显示的屏幕所对应的userId刚好就是这次启动指定的userId就直接返回上次的屏幕区域
            if (userForDisplay == userId) {
                return candidate;
            }
        }
        return null;
    }
```

如果启动的Activity是首次启动，或者其上一次启动所在的屏幕所对应的userId不等于启动时所指定的userId就会调用`fallbackDisplayArea`去获取其他的屏幕：

```
    @Nullable
    private TaskDisplayArea fallbackDisplayArea(int userId) {
        //根据userId获取屏幕ID，当一个userId对应多块屏幕时，从mDefaultDisplayForProfileUser里面得到的是第一块屏幕
        int displayIdForUserProfile = mDefaultDisplayForProfileUser.get(userId,
                Display.INVALID_DISPLAY);
        if (displayIdForUserProfile != Display.INVALID_DISPLAY) {
            int displayId = mDefaultDisplayForProfileUser.get(userId);
            //得到屏幕所对应的TaskDisplayArea
            return getDefaultTaskDisplayAreaOnDisplay(displayId);
        }
       //如果mDefaultDisplayForProfileUser中没有userId对应的屏幕，并且此时系统有乘客对应的屏幕
        if (!mPassengerDisplays.isEmpty()) {
            //这里直接得到乘客所有屏幕中的第一块
            int displayId = mPassengerDisplays.get(0);
            return getDefaultTaskDisplayAreaOnDisplay(displayId);
        }

        return null;
    }
```

`fallbackDisplayArea`优先根据userId从`mDefaultDisplayForProfileUser`获取其对应的第一块屏幕，如果没有获取到，则会获取乘客的所有屏幕中的第一块，如果也没有直接返回null，后续将会在默认display中显示这个Activity。

### 3\. 流程图

最后附上流程图：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9c99c8f01ab1c74a1da1074450f9cc32.png#pic_center)