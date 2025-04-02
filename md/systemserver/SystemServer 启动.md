```
handleSystemServerProcess(){

	ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,  
        parsedArgs.mRemainingArgs, cl) {
			RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader){
				findStaticMain(args.startClass, args.startArgs, classLoader){
					new MethodAndArgsCaller(m, argv){
						public void run() {  
						    try {  
							    // systemserver.main.run()
						        mMethod.invoke(null, new Object[] { mArgs });  
						    } catch (IllegalAccessException ex) {  
						        throw new RuntimeException(ex);  
						    } catch (InvocationTargetException ex) {  
						        Throwable cause = ex.getCause();  
						        if (cause instanceof RuntimeException) {  
						            throw (RuntimeException) cause;  
						        } else if (cause instanceof Error) {  
						            throw (Error) cause;  
						        }  
						        throw new RuntimeException(ex);  
						    }  
						}
					}
				}
			}       
        }
}
```


```
private void run() {  
    try {  
        
        createSystemContext();  
  
        // Create the system service manager.  
        mSystemServiceManager = new SystemServiceManager(mSystemContext);  
        mSystemServiceManager.setStartInfo(mRuntimeRestart,  
                mRuntimeStartElapsedTime, mRuntimeStartUptime);  
        LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);  
        // Prepare the thread pool for init tasks that can be parallelized  
        SystemServerInitThreadPool.get();  
    } finally {  
        traceEnd();  // InitBeforeStartServices  
    }  
  
    // Start services.  
    try {  
        traceBeginAndSlog("StartServices");  
        
        startBootstrapServices();  
        startCoreServices();  
        startOtherServices();  
    
        SystemServerInitThreadPool.shutdown();  
    } catch (Throwable ex) {  
        Slog.e("System", "******************************************");  
        Slog.e("System", "************ Failure starting system services", ex);  
        throw ex;  
    } finally {  
        traceEnd();  
    }  
  
    // Loop forever.  
    Looper.loop();  
    throw new RuntimeException("Main thread loop unexpectedly exited");  
}

```

```
private void startBootstrapServices() {  
   
}
```

```
private void startCoreServices() {

}
```

```
private void startOtherServices() {  
    final Context context = mSystemContext;  
    VibratorService vibrator = null;  
    DynamicSystemService dynamicSystem = null;  
    IStorageManager storageManager = null;  
    NetworkManagementService networkManagement = null;  
    IpSecService ipSecService = null;  
    NetworkStatsService networkStats = null;  
    NetworkPolicyManagerService networkPolicy = null;  
    ConnectivityService connectivity = null;  
    NsdService serviceDiscovery = null;  
    WindowManagerService wm = null;  
    SerialService serial = null;  
    NetworkTimeUpdateService networkTimeUpdater = null;  
    InputManagerService inputManager = null;  
    TelephonyRegistry telephonyRegistry = null;  
    ConsumerIrService consumerIr = null;  
    MmsServiceBroker mmsService = null;  
    HardwarePropertiesManagerService hardwarePropertiesService = null;  
    CellsService cellsService = null;  
  
    boolean disableSystemTextClassifier = SystemProperties.getBoolean(  
            "config.disable_systemtextclassifier", false);  
  
    boolean disableNetworkTime = SystemProperties.getBoolean("config.disable_networktime",  
            false);  
    boolean disableCameraService = SystemProperties.getBoolean("config.disable_cameraservice",  
            false);  
    boolean disableSlices = SystemProperties.getBoolean("config.disable_slices", false);  
    boolean enableLeftyService = SystemProperties.getBoolean("config.enable_lefty", false);  
  
    boolean isEmulator = SystemProperties.get("ro.kernel.qemu").equals("1");  
  
    boolean isWatch = context.getPackageManager().hasSystemFeature(  
            PackageManager.FEATURE_WATCH);  
  
    boolean isArc = context.getPackageManager().hasSystemFeature(  
            "org.chromium.arc");  
  
    boolean enableVrService = context.getPackageManager().hasSystemFeature(  
            PackageManager.FEATURE_VR_MODE_HIGH_PERFORMANCE);  
  
    // For debugging RescueParty  
    if (Build.IS_DEBUGGABLE && SystemProperties.getBoolean("debug.crash_system", false)) {  
        throw new RuntimeException();  
    }  
  
    try {  
        final String SECONDARY_ZYGOTE_PRELOAD = "SecondaryZygotePreload";  
        // We start the preload ~1s before the webview factory preparation, to  
        // ensure that it completes before the 32 bit relro process is forked        // from the zygote. In the event that it takes too long, the webview        // RELRO process will block, but it will do so without holding any locks.        mZygotePreload = SystemServerInitThreadPool.get().submit(() -> {  
            try {  
                Slog.i(TAG, SECONDARY_ZYGOTE_PRELOAD);  
                TimingsTraceLog traceLog = new TimingsTraceLog(  
                        SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);  
                traceLog.traceBegin(SECONDARY_ZYGOTE_PRELOAD);  
                if (!Process.ZYGOTE_PROCESS.preloadDefault(Build.SUPPORTED_32_BIT_ABIS[0])) {  
                    Slog.e(TAG, "Unable to preload default resources");  
                }  
                traceLog.traceEnd();  
            } catch (Exception ex) {  
                Slog.e(TAG, "Exception preloading default resources", ex);  
            }  
        }, SECONDARY_ZYGOTE_PRELOAD);  
  
        traceBeginAndSlog("StartKeyAttestationApplicationIdProviderService");  
        ServiceManager.addService("sec_key_att_app_id_provider",  
                new KeyAttestationApplicationIdProviderService(context));  
        traceEnd();  
  
        traceBeginAndSlog("StartKeyChainSystemService");  
        mSystemServiceManager.startService(KeyChainSystemService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartSchedulingPolicyService");  
        ServiceManager.addService("scheduling_policy", new SchedulingPolicyService());  
        traceEnd();  
  
        traceBeginAndSlog("StartTelecomLoaderService");  
        mSystemServiceManager.startService(TelecomLoaderService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartTelephonyRegistry");  
        telephonyRegistry = new TelephonyRegistry(context);  
        ServiceManager.addService("telephony.registry", telephonyRegistry);  
        traceEnd();  
  
        traceBeginAndSlog("StartEntropyMixer");  
        mEntropyMixer = new EntropyMixer(context);  
        traceEnd();  
  
        mContentResolver = context.getContentResolver();  
  
        // The AccountManager must come before the ContentService  
        traceBeginAndSlog("StartAccountManagerService");  
        mSystemServiceManager.startService(ACCOUNT_SERVICE_CLASS);  
        traceEnd();  
  
        traceBeginAndSlog("StartContentService");  
        mSystemServiceManager.startService(CONTENT_SERVICE_CLASS);  
        traceEnd();  
  
        traceBeginAndSlog("InstallSystemProviders");  
        mActivityManagerService.installSystemProviders();  
        // Now that SettingsProvider is ready, reactivate SQLiteCompatibilityWalFlags  
        SQLiteCompatibilityWalFlags.reset();  
        traceEnd();  
  
        // Records errors and logs, for example wtf()  
        // Currently this service indirectly depends on SettingsProvider so do this after        // InstallSystemProviders.        traceBeginAndSlog("StartDropBoxManager");  
        mSystemServiceManager.startService(DropBoxManagerService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartVibratorService");  
        vibrator = new VibratorService(context);  
        ServiceManager.addService("vibrator", vibrator);  
        traceEnd();  
  
        traceBeginAndSlog("StartDynamicSystemService");  
        dynamicSystem = new DynamicSystemService(context);  
        ServiceManager.addService("dynamic_system", dynamicSystem);  
        traceEnd();  
  
        if (!isWatch) {  
            traceBeginAndSlog("StartConsumerIrService");  
            consumerIr = new ConsumerIrService(context);  
            ServiceManager.addService(Context.CONSUMER_IR_SERVICE, consumerIr);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartAlarmManagerService");  
        mSystemServiceManager.startService(new AlarmManagerService(context));  
        traceEnd();  
  
        traceBeginAndSlog("StartInputManagerService");  
        inputManager = new InputManagerService(context);  
        traceEnd();  
  
        traceBeginAndSlog("StartWindowManagerService");  
        // WMS needs sensor service ready  
        ConcurrentUtils.waitForFutureNoInterrupt(mSensorServiceStart, START_SENSOR_SERVICE);  
        mSensorServiceStart = null;  
        wm = WindowManagerService.main(context, inputManager, !mFirstBoot, mOnlyCore,  
                new PhoneWindowManager(), mActivityManagerService.mActivityTaskManager);  
        ServiceManager.addService(Context.WINDOW_SERVICE, wm, /* allowIsolated= */ false,  
                DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PROTO);  
        ServiceManager.addService(Context.INPUT_SERVICE, inputManager,  
                /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);  
        traceEnd();  
  
        traceBeginAndSlog("SetWindowManagerService");  
        mActivityManagerService.setWindowManager(wm);  
        traceEnd();  
  
        traceBeginAndSlog("WindowManagerServiceOnInitReady");  
        wm.onInitReady();  
        traceEnd();  
  
        // Start receiving calls from HIDL services. Start in in a separate thread  
        // because it need to connect to SensorManager. This have to start        // after START_SENSOR_SERVICE is done.        if(SystemProperties.get("ro.boot.vm","0").equals("0")){  
        SystemServerInitThreadPool.get().submit(() -> {  
            TimingsTraceLog traceLog = new TimingsTraceLog(  
                    SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);  
            traceLog.traceBegin(START_HIDL_SERVICES);  
            startHidlServices();  
            traceLog.traceEnd();  
        }, START_HIDL_SERVICES);  
        }  
  
        if (!isWatch && enableVrService) {  
            traceBeginAndSlog("StartVrManagerService");  
            mSystemServiceManager.startService(VrManagerService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartInputManager");  
        inputManager.setWindowManagerCallbacks(wm.getInputManagerCallback());  
        inputManager.start();  
        traceEnd();  
  
        // TODO: Use service dependencies instead.  
        traceBeginAndSlog("DisplayManagerWindowManagerAndInputReady");  
        mDisplayManagerService.windowManagerAndInputReady();  
        traceEnd();  
  
        if (mFactoryTestMode == FactoryTest.FACTORY_TEST_LOW_LEVEL) {  
            Slog.i(TAG, "No Bluetooth Service (factory test)");  
        } else if (!context.getPackageManager().hasSystemFeature  
                (PackageManager.FEATURE_BLUETOOTH)) {  
            Slog.i(TAG, "No Bluetooth Service (Bluetooth Hardware Not Present)");  
        } else {  
            traceBeginAndSlog("StartBluetoothService");  
            mSystemServiceManager.startService(BluetoothService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("IpConnectivityMetrics");  
        mSystemServiceManager.startService(IpConnectivityMetrics.class);  
        traceEnd();  
  
        traceBeginAndSlog("NetworkWatchlistService");  
        mSystemServiceManager.startService(NetworkWatchlistService.Lifecycle.class);  
        traceEnd();  
  
        traceBeginAndSlog("PinnerService");  
        mSystemServiceManager.startService(PinnerService.class);  
        traceEnd();  
  
        traceBeginAndSlog("SignedConfigService");  
        SignedConfigService.registerUpdateReceiver(mSystemContext);  
        traceEnd();  
    } catch (RuntimeException e) {  
        Slog.e("System", "******************************************");  
        Slog.e("System", "************ Failure starting core service", e);  
    }  
  
    // Before things start rolling, be sure we have decided whether  
    // we are in safe mode.    final boolean safeMode = wm.detectSafeMode();  
    if (safeMode) {  
        // If yes, immediately turn on the global setting for airplane mode.  
        // Note that this does not send broadcasts at this stage because        // subsystems are not yet up. We will send broadcasts later to ensure        // all listeners have the chance to react with special handling.        Settings.Global.putInt(context.getContentResolver(),  
                Settings.Global.AIRPLANE_MODE_ON, 1);  
    }  
  
    StatusBarManagerService statusBar = null;  
    INotificationManager notification = null;  
    LocationManagerService location = null;  
    CountryDetectorService countryDetector = null;  
    ILockSettings lockSettings = null;  
    MediaRouterService mediaRouter = null;  
  
    // Bring up services needed for UI.  
    if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {  
        traceBeginAndSlog("StartInputMethodManagerLifecycle");  
        if (InputMethodSystemProperty.MULTI_CLIENT_IME_ENABLED) {  
            mSystemServiceManager.startService(  
                    MultiClientInputMethodManagerService.Lifecycle.class);  
        } else {  
            mSystemServiceManager.startService(InputMethodManagerService.Lifecycle.class);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartAccessibilityManagerService");  
        try {  
            mSystemServiceManager.startService(ACCESSIBILITY_MANAGER_SERVICE_CLASS);  
        } catch (Throwable e) {  
            reportWtf("starting Accessibility Manager", e);  
        }  
        traceEnd();  
    }  
  
    traceBeginAndSlog("MakeDisplayReady");  
    try {  
        wm.displayReady();  
    } catch (Throwable e) {  
        reportWtf("making display ready", e);  
    }  
    traceEnd();  
  
    if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {  
        if (!"0".equals(SystemProperties.get("system_init.startmountservice"))) {  
            traceBeginAndSlog("StartStorageManagerService");  
            try {  
                /*  
                 * NotificationManagerService is dependant on StorageManagerService,                 * (for media / usb notifications) so we must start StorageManagerService first.                 */                mSystemServiceManager.startService(STORAGE_MANAGER_SERVICE_CLASS);  
                storageManager = IStorageManager.Stub.asInterface(  
                        ServiceManager.getService("mount"));  
            } catch (Throwable e) {  
                reportWtf("starting StorageManagerService", e);  
            }  
            traceEnd();  
  
            traceBeginAndSlog("StartStorageStatsService");  
            try {  
                mSystemServiceManager.startService(STORAGE_STATS_SERVICE_CLASS);  
            } catch (Throwable e) {  
                reportWtf("starting StorageStatsService", e);  
            }  
            traceEnd();  
        }  
    }  
  
    // We start this here so that we update our configuration to set watch or television  
    // as appropriate.    traceBeginAndSlog("StartUiModeManager");  
    mSystemServiceManager.startService(UiModeManagerService.class);  
    traceEnd();  
  
    if (!mOnlyCore) {  
        traceBeginAndSlog("UpdatePackagesIfNeeded");  
        try {  
            Watchdog.getInstance().pauseWatchingCurrentThread("dexopt");  
            mPackageManagerService.updatePackagesIfNeeded();  
        } catch (Throwable e) {  
            reportWtf("update packages", e);  
        } finally {  
            Watchdog.getInstance().resumeWatchingCurrentThread("dexopt");  
        }  
        traceEnd();  
    }  
  
    traceBeginAndSlog("PerformFstrimIfNeeded");  
    try {  
        mPackageManagerService.performFstrimIfNeeded();  
    } catch (Throwable e) {  
        reportWtf("performing fstrim", e);  
    }  
    traceEnd();  
  
    if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {  
        traceBeginAndSlog("StartLockSettingsService");  
        try {  
            mSystemServiceManager.startService(LOCK_SETTINGS_SERVICE_CLASS);  
            lockSettings = ILockSettings.Stub.asInterface(  
                    ServiceManager.getService("lock_settings"));  
        } catch (Throwable e) {  
            reportWtf("starting LockSettingsService service", e);  
        }  
        traceEnd();  
  
        final boolean hasPdb = !SystemProperties.get(PERSISTENT_DATA_BLOCK_PROP).equals("");  
        final boolean hasGsi = SystemProperties.getInt(GSI_RUNNING_PROP, 0) > 0;  
        if (hasPdb && !hasGsi) {  
            traceBeginAndSlog("StartPersistentDataBlock");  
            mSystemServiceManager.startService(PersistentDataBlockService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartTestHarnessMode");  
        mSystemServiceManager.startService(TestHarnessModeService.class);  
        traceEnd();  
  
        if (hasPdb || OemLockService.isHalPresent()) {  
            // Implementation depends on pdb or the OemLock HAL  
            traceBeginAndSlog("StartOemLockService");  
            mSystemServiceManager.startService(OemLockService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartDeviceIdleController");  
        mSystemServiceManager.startService(DeviceIdleController.class);  
        traceEnd();  
  
        // Always start the Device Policy Manager, so that the API is compatible with  
        // API8.        traceBeginAndSlog("StartDevicePolicyManager");  
        mSystemServiceManager.startService(DevicePolicyManagerService.Lifecycle.class);  
        traceEnd();  
  
        if (!isWatch) {  
            traceBeginAndSlog("StartStatusBarManagerService");  
            try {  
                statusBar = new StatusBarManagerService(context, wm);  
                ServiceManager.addService(Context.STATUS_BAR_SERVICE, statusBar);  
            } catch (Throwable e) {  
                reportWtf("starting StatusBarManagerService", e);  
            }  
            traceEnd();  
        }  
  
        startContentCaptureService(context);  
        startAttentionService(context);  
  
        startSystemCaptionsManagerService(context);  
  
        // App prediction manager service  
        if (deviceHasConfigString(context, R.string.config_defaultAppPredictionService)) {  
            traceBeginAndSlog("StartAppPredictionService");  
            mSystemServiceManager.startService(APP_PREDICTION_MANAGER_SERVICE_CLASS);  
            traceEnd();  
        } else {  
            Slog.d(TAG, "AppPredictionService not defined by OEM");  
        }  
  
        // Content suggestions manager service  
        if (deviceHasConfigString(context, R.string.config_defaultContentSuggestionsService)) {  
            traceBeginAndSlog("StartContentSuggestionsService");  
            mSystemServiceManager.startService(CONTENT_SUGGESTIONS_SERVICE_CLASS);  
            traceEnd();  
        } else {  
            Slog.d(TAG, "ContentSuggestionsService not defined by OEM");  
        }  
  
        traceBeginAndSlog("InitNetworkStackClient");  
        try {  
            NetworkStackClient.getInstance().init();  
        } catch (Throwable e) {  
            reportWtf("initializing NetworkStackClient", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartNetworkManagementService");  
        try {  
            networkManagement = NetworkManagementService.create(context);  
            ServiceManager.addService(Context.NETWORKMANAGEMENT_SERVICE, networkManagement);  
        } catch (Throwable e) {  
            reportWtf("starting NetworkManagement Service", e);  
        }  
        traceEnd();  
  
  
        traceBeginAndSlog("StartIpSecService");  
        try {  
            ipSecService = IpSecService.create(context);  
            ServiceManager.addService(Context.IPSEC_SERVICE, ipSecService);  
        } catch (Throwable e) {  
            reportWtf("starting IpSec Service", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartTextServicesManager");  
        mSystemServiceManager.startService(TextServicesManagerService.Lifecycle.class);  
        traceEnd();  
  
        if (!disableSystemTextClassifier) {  
            traceBeginAndSlog("StartTextClassificationManagerService");  
            mSystemServiceManager  
                    .startService(TextClassificationManagerService.Lifecycle.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartNetworkScoreService");  
        mSystemServiceManager.startService(NetworkScoreService.Lifecycle.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartNetworkStatsService");  
        try {  
            networkStats = NetworkStatsService.create(context, networkManagement);  
            ServiceManager.addService(Context.NETWORK_STATS_SERVICE, networkStats);  
        } catch (Throwable e) {  
            reportWtf("starting NetworkStats Service", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartNetworkPolicyManagerService");  
        try {  
            networkPolicy = new NetworkPolicyManagerService(context, mActivityManagerService,  
                    networkManagement);  
            ServiceManager.addService(Context.NETWORK_POLICY_SERVICE, networkPolicy);  
        } catch (Throwable e) {  
            reportWtf("starting NetworkPolicy Service", e);  
        }  
        traceEnd();  
  
        if(SystemProperties.get("ro.boot.vm","0").equals("0")){  
        if (context.getPackageManager().hasSystemFeature(  
                PackageManager.FEATURE_WIFI)) {  
            // Wifi Service must be started first for wifi-related services.  
            traceBeginAndSlog("StartWifi");  
            mSystemServiceManager.startService(WIFI_SERVICE_CLASS);  
            traceEnd();  
            traceBeginAndSlog("StartWifiScanning");  
            mSystemServiceManager.startService(  
                    "com.android.server.wifi.scanner.WifiScanningService");  
            traceEnd();  
        }  
  
        if (context.getPackageManager().hasSystemFeature(  
                PackageManager.FEATURE_WIFI_RTT)) {  
            traceBeginAndSlog("StartRttService");  
            mSystemServiceManager.startService(  
                    "com.android.server.wifi.rtt.RttService");  
            traceEnd();  
        }  
  
        if (context.getPackageManager().hasSystemFeature(  
                PackageManager.FEATURE_WIFI_AWARE)) {  
            traceBeginAndSlog("StartWifiAware");  
            mSystemServiceManager.startService(WIFI_AWARE_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (context.getPackageManager().hasSystemFeature(  
                PackageManager.FEATURE_WIFI_DIRECT)) {  
            traceBeginAndSlog("StartWifiP2P");  
            mSystemServiceManager.startService(WIFI_P2P_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (context.getPackageManager().hasSystemFeature(  
                PackageManager.FEATURE_LOWPAN)) {  
            traceBeginAndSlog("StartLowpan");  
            mSystemServiceManager.startService(LOWPAN_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_ETHERNET) ||  
                mPackageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST)) {  
            traceBeginAndSlog("StartEthernet");  
            mSystemServiceManager.startService(ETHERNET_SERVICE_CLASS);  
            traceEnd();  
        }  
        }  
  
        traceBeginAndSlog("StartConnectivityService");  
        try {  
            connectivity = new ConnectivityService(  
                    context, networkManagement, networkStats, networkPolicy);  
            ServiceManager.addService(Context.CONNECTIVITY_SERVICE, connectivity,  
                    /* allowIsolated= */ false,  
                    DUMP_FLAG_PRIORITY_HIGH | DUMP_FLAG_PRIORITY_NORMAL);  
            networkPolicy.bindConnectivityManager(connectivity);  
        } catch (Throwable e) {  
            reportWtf("starting Connectivity Service", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartNsdService");  
        try {  
            serviceDiscovery = NsdService.create(context);  
            ServiceManager.addService(  
                    Context.NSD_SERVICE, serviceDiscovery);  
        } catch (Throwable e) {  
            reportWtf("starting Service Discovery Service", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartSystemUpdateManagerService");  
        try {  
            ServiceManager.addService(Context.SYSTEM_UPDATE_SERVICE,  
                    new SystemUpdateManagerService(context));  
        } catch (Throwable e) {  
            reportWtf("starting SystemUpdateManagerService", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartUpdateLockService");  
        try {  
            ServiceManager.addService(Context.UPDATE_LOCK_SERVICE,  
                    new UpdateLockService(context));  
        } catch (Throwable e) {  
            reportWtf("starting UpdateLockService", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartNotificationManager");  
        mSystemServiceManager.startService(NotificationManagerService.class);  
        SystemNotificationChannels.removeDeprecated(context);  
        SystemNotificationChannels.createAll(context);  
        notification = INotificationManager.Stub.asInterface(  
                ServiceManager.getService(Context.NOTIFICATION_SERVICE));  
        traceEnd();  
  
        traceBeginAndSlog("StartDeviceMonitor");  
        mSystemServiceManager.startService(DeviceStorageMonitorService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartLocationManagerService");  
        try {  
            location = new LocationManagerService(context);  
            ServiceManager.addService(Context.LOCATION_SERVICE, location);  
        } catch (Throwable e) {  
            reportWtf("starting Location Manager", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartCountryDetectorService");  
        try {  
            countryDetector = new CountryDetectorService(context);  
            ServiceManager.addService(Context.COUNTRY_DETECTOR, countryDetector);  
        } catch (Throwable e) {  
            reportWtf("starting Country Detector", e);  
        }  
        traceEnd();  
  
        final boolean useNewTimeServices = true;  
        if (useNewTimeServices) {  
            traceBeginAndSlog("StartTimeDetectorService");  
            try {  
                mSystemServiceManager.startService(TIME_DETECTOR_SERVICE_CLASS);  
            } catch (Throwable e) {  
                reportWtf("starting StartTimeDetectorService service", e);  
            }  
            traceEnd();  
        }  
  
        if (!isWatch) {  
            traceBeginAndSlog("StartSearchManagerService");  
            try {  
                mSystemServiceManager.startService(SEARCH_MANAGER_SERVICE_CLASS);  
            } catch (Throwable e) {  
                reportWtf("starting Search Service", e);  
            }  
            traceEnd();  
        }  
  
        if (context.getResources().getBoolean(R.bool.config_enableWallpaperService)) {  
            traceBeginAndSlog("StartWallpaperManagerService");  
            mSystemServiceManager.startService(WALLPAPER_SERVICE_CLASS);  
            traceEnd();  
        } else {  
            Slog.i(TAG, "Wallpaper service disabled by config");  
        }  
  
        traceBeginAndSlog("StartAudioService");  
        if (!isArc) {  
            mSystemServiceManager.startService(AudioService.Lifecycle.class);  
        } else {  
            String className = context.getResources()  
                    .getString(R.string.config_deviceSpecificAudioService);  
            try {  
                mSystemServiceManager.startService(className + "$Lifecycle");  
            } catch (Throwable e) {  
                reportWtf("starting " + className, e);  
            }  
        }  
        traceEnd();  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_BROADCAST_RADIO)) {  
            traceBeginAndSlog("StartBroadcastRadioService");  
            mSystemServiceManager.startService(BroadcastRadioService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartDockObserver");  
        mSystemServiceManager.startService(DockObserver.class);  
        traceEnd();  
  
        if (isWatch) {  
            traceBeginAndSlog("StartThermalObserver");  
            mSystemServiceManager.startService(THERMAL_OBSERVER_CLASS);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartWiredAccessoryManager");  
        try {  
            // Listen for wired headset changes  
            inputManager.setWiredAccessoryCallbacks(  
                    new WiredAccessoryManager(context, inputManager));  
        } catch (Throwable e) {  
            reportWtf("starting WiredAccessoryManager", e);  
        }  
        traceEnd();  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_MIDI)) {  
            // Start MIDI Manager service  
            traceBeginAndSlog("StartMidiManager");  
            mSystemServiceManager.startService(MIDI_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        // Start ADB Debugging Service  
        traceBeginAndSlog("StartAdbService");  
        try {  
            mSystemServiceManager.startService(ADB_SERVICE_CLASS);  
        } catch (Throwable e) {  
            Slog.e(TAG, "Failure starting AdbService");  
        }  
        traceEnd();  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST)  
                || mPackageManager.hasSystemFeature(  
                PackageManager.FEATURE_USB_ACCESSORY)  
                || isEmulator) {  
            // Manage USB host and device support  
            traceBeginAndSlog("StartUsbService");  
            mSystemServiceManager.startService(USB_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (!isWatch) {  
            traceBeginAndSlog("StartSerialService");  
            try {  
                // Serial port support  
                serial = new SerialService(context);  
                ServiceManager.addService(Context.SERIAL_SERVICE, serial);  
            } catch (Throwable e) {  
                Slog.e(TAG, "Failure starting SerialService", e);  
            }  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartHardwarePropertiesManagerService");  
        try {  
            hardwarePropertiesService = new HardwarePropertiesManagerService(context);  
            ServiceManager.addService(Context.HARDWARE_PROPERTIES_SERVICE,  
                    hardwarePropertiesService);  
        } catch (Throwable e) {  
            Slog.e(TAG, "Failure starting HardwarePropertiesManagerService", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("StartTwilightService");  
        mSystemServiceManager.startService(TwilightService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartColorDisplay");  
        mSystemServiceManager.startService(ColorDisplayService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartJobScheduler");  
        mSystemServiceManager.startService(JobSchedulerService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartSoundTrigger");  
        mSystemServiceManager.startService(SoundTriggerService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartTrustManager");  
        mSystemServiceManager.startService(TrustManagerService.class);  
        traceEnd();  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_BACKUP)) {  
            traceBeginAndSlog("StartBackupManager");  
            mSystemServiceManager.startService(BACKUP_MANAGER_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_APP_WIDGETS)  
                || context.getResources().getBoolean(R.bool.config_enableAppWidgetService)) {  
            traceBeginAndSlog("StartAppWidgetService");  
            mSystemServiceManager.startService(APPWIDGET_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        // Grants default permissions and defines roles  
        traceBeginAndSlog("StartRoleManagerService");  
        mSystemServiceManager.startService(new RoleManagerService(  
                mSystemContext, new LegacyRoleResolutionPolicy(mSystemContext)));  
        traceEnd();  
  
        // We need to always start this service, regardless of whether the  
        // FEATURE_VOICE_RECOGNIZERS feature is set, because it needs to take care        // of initializing various settings.  It will internally modify its behavior        // based on that feature.        traceBeginAndSlog("StartVoiceRecognitionManager");  
        mSystemServiceManager.startService(VOICE_RECOGNITION_MANAGER_SERVICE_CLASS);  
        traceEnd();  
  
        if (GestureLauncherService.isGestureLauncherEnabled(context.getResources())) {  
            traceBeginAndSlog("StartGestureLauncher");  
            mSystemServiceManager.startService(GestureLauncherService.class);  
            traceEnd();  
        }  
        traceBeginAndSlog("StartSensorNotification");  
        mSystemServiceManager.startService(SensorNotificationService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartContextHubSystemService");  
        mSystemServiceManager.startService(ContextHubSystemService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartDiskStatsService");  
        try {  
            ServiceManager.addService("diskstats", new DiskStatsService(context));  
        } catch (Throwable e) {  
            reportWtf("starting DiskStats Service", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("RuntimeService");  
        try {  
            ServiceManager.addService("runtime", new RuntimeService(context));  
        } catch (Throwable e) {  
            reportWtf("starting RuntimeService", e);  
        }  
        traceEnd();  
  
        // timezone.RulesManagerService will prevent a device starting up if the chain of trust  
        // required for safe time zone updates might be broken. RuleManagerService cannot do        // this check when mOnlyCore == true, so we don't enable the service in this case.        // This service requires that JobSchedulerService is already started when it starts.        final boolean startRulesManagerService =  
                !mOnlyCore && context.getResources().getBoolean(  
                        R.bool.config_enableUpdateableTimeZoneRules);  
        if (startRulesManagerService) {  
            traceBeginAndSlog("StartTimeZoneRulesManagerService");  
            mSystemServiceManager.startService(TIME_ZONE_RULES_MANAGER_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (!isWatch && !disableNetworkTime) {  
            traceBeginAndSlog("StartNetworkTimeUpdateService");  
            try {  
                if (useNewTimeServices) {  
                    networkTimeUpdater = new NewNetworkTimeUpdateService(context);  
                } else {  
                    networkTimeUpdater = new OldNetworkTimeUpdateService(context);  
                }  
                Slog.d(TAG, "Using networkTimeUpdater class=" + networkTimeUpdater.getClass());  
                ServiceManager.addService("network_time_update_service", networkTimeUpdater);  
            } catch (Throwable e) {  
                reportWtf("starting NetworkTimeUpdate service", e);  
            }  
            traceEnd();  
        }  
  
        traceBeginAndSlog("CertBlacklister");  
        try {  
            CertBlacklister blacklister = new CertBlacklister(context);  
        } catch (Throwable e) {  
            reportWtf("starting CertBlacklister", e);  
        }  
        traceEnd();  
  
        if (EmergencyAffordanceManager.ENABLED) {  
            // EmergencyMode service  
            traceBeginAndSlog("StartEmergencyAffordanceService");  
            mSystemServiceManager.startService(EmergencyAffordanceService.class);  
            traceEnd();  
        }  
  
        // Dreams (interactive idle-time views, a/k/a screen savers, and doze mode)  
        traceBeginAndSlog("StartDreamManager");  
        mSystemServiceManager.startService(DreamManagerService.class);  
        traceEnd();  
  
        traceBeginAndSlog("AddGraphicsStatsService");  
        ServiceManager.addService(GraphicsStatsService.GRAPHICS_STATS_SERVICE,  
                new GraphicsStatsService(context));  
        traceEnd();  
  
        if (CoverageService.ENABLED) {  
            traceBeginAndSlog("AddCoverageService");  
            ServiceManager.addService(CoverageService.COVERAGE_SERVICE, new CoverageService());  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_PRINTING)) {  
            traceBeginAndSlog("StartPrintManager");  
            mSystemServiceManager.startService(PRINT_MANAGER_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_COMPANION_DEVICE_SETUP)) {  
            traceBeginAndSlog("StartCompanionDeviceManager");  
            mSystemServiceManager.startService(COMPANION_DEVICE_MANAGER_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartRestrictionManager");  
        mSystemServiceManager.startService(RestrictionsManagerService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartMediaSessionService");  
        mSystemServiceManager.startService(MediaSessionService.class);  
        traceEnd();  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_HDMI_CEC)) {  
            traceBeginAndSlog("StartHdmiControlService");  
            mSystemServiceManager.startService(HdmiControlService.class);  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_LIVE_TV)  
                || mPackageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)) {  
            traceBeginAndSlog("StartTvInputManager");  
            mSystemServiceManager.startService(TvInputManagerService.class);  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {  
            traceBeginAndSlog("StartMediaResourceMonitor");  
            mSystemServiceManager.startService(MediaResourceMonitorService.class);  
            traceEnd();  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)) {  
            traceBeginAndSlog("StartTvRemoteService");  
            mSystemServiceManager.startService(TvRemoteService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartMediaRouterService");  
        try {  
            mediaRouter = new MediaRouterService(context);  
            ServiceManager.addService(Context.MEDIA_ROUTER_SERVICE, mediaRouter);  
        } catch (Throwable e) {  
            reportWtf("starting MediaRouterService", e);  
        }  
        traceEnd();  
  
        final boolean hasFeatureFace  
                = mPackageManager.hasSystemFeature(PackageManager.FEATURE_FACE);  
        final boolean hasFeatureIris  
                = mPackageManager.hasSystemFeature(PackageManager.FEATURE_IRIS);  
        final boolean hasFeatureFingerprint  
                = mPackageManager.hasSystemFeature(PackageManager.FEATURE_FINGERPRINT);  
  
        if (hasFeatureFace) {  
            traceBeginAndSlog("StartFaceSensor");  
            mSystemServiceManager.startService(FaceService.class);  
            traceEnd();  
        }  
  
        if (hasFeatureIris) {  
            traceBeginAndSlog("StartIrisSensor");  
            mSystemServiceManager.startService(IrisService.class);  
            traceEnd();  
        }  
  
        if (hasFeatureFingerprint) {  
            traceBeginAndSlog("StartFingerprintSensor");  
            mSystemServiceManager.startService(FingerprintService.class);  
            traceEnd();  
        }  
  
        if (hasFeatureFace || hasFeatureIris || hasFeatureFingerprint) {  
            // Start this service after all biometric services.  
            traceBeginAndSlog("StartBiometricService");  
            mSystemServiceManager.startService(BiometricService.class);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartBackgroundDexOptService");  
        try {  
            BackgroundDexOptService.schedule(context);  
        } catch (Throwable e) {  
            reportWtf("starting StartBackgroundDexOptService", e);  
        }  
        traceEnd();  
  
        if (!isWatch) {  
            // We don't run this on watches as there are no plans to use the data logged  
            // on watch devices.            traceBeginAndSlog("StartDynamicCodeLoggingService");  
            try {  
                DynamicCodeLoggingService.schedule(context);  
            } catch (Throwable e) {  
                reportWtf("starting DynamicCodeLoggingService", e);  
            }  
            traceEnd();  
        }  
  
        if (!isWatch) {  
            traceBeginAndSlog("StartPruneInstantAppsJobService");  
            try {  
                PruneInstantAppsJobService.schedule(context);  
            } catch (Throwable e) {  
                reportWtf("StartPruneInstantAppsJobService", e);  
            }  
            traceEnd();  
        }  
  
        // LauncherAppsService uses ShortcutService.  
        traceBeginAndSlog("StartShortcutServiceLifecycle");  
        mSystemServiceManager.startService(ShortcutService.Lifecycle.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartLauncherAppsService");  
        mSystemServiceManager.startService(LauncherAppsService.class);  
        traceEnd();  
  
        traceBeginAndSlog("StartCrossProfileAppsService");  
        mSystemServiceManager.startService(CrossProfileAppsService.class);  
        traceEnd();  
  
        try {  
            Slog.i(TAG, "cells Service");  
            cellsService = new CellsService(context);  
            ServiceManager.addService(Context.CELLS_SERVICE,cellsService);  
        } catch (Throwable e) {  
            reportWtf("starting CellsService", e);  
        }  
  
    }  
  
    if (!isWatch) {  
        traceBeginAndSlog("StartMediaProjectionManager");  
        mSystemServiceManager.startService(MediaProjectionManagerService.class);  
        traceEnd();  
    }  
  
    if (isWatch) {  
        // Must be started before services that depend it, e.g. WearConnectivityService  
        traceBeginAndSlog("StartWearPowerService");  
        mSystemServiceManager.startService(WEAR_POWER_SERVICE_CLASS);  
        traceEnd();  
  
        traceBeginAndSlog("StartWearConnectivityService");  
        mSystemServiceManager.startService(WEAR_CONNECTIVITY_SERVICE_CLASS);  
        traceEnd();  
  
        traceBeginAndSlog("StartWearDisplayService");  
        mSystemServiceManager.startService(WEAR_DISPLAY_SERVICE_CLASS);  
        traceEnd();  
  
        traceBeginAndSlog("StartWearTimeService");  
        mSystemServiceManager.startService(WEAR_TIME_SERVICE_CLASS);  
        traceEnd();  
  
        if (enableLeftyService) {  
            traceBeginAndSlog("StartWearLeftyService");  
            mSystemServiceManager.startService(WEAR_LEFTY_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartWearGlobalActionsService");  
        mSystemServiceManager.startService(WEAR_GLOBAL_ACTIONS_SERVICE_CLASS);  
        traceEnd();  
    }  
  
    if (!disableSlices) {  
        traceBeginAndSlog("StartSliceManagerService");  
        mSystemServiceManager.startService(SLICE_MANAGER_SERVICE_CLASS);  
        traceEnd();  
    }  
  
    if (!disableCameraService) {  
        traceBeginAndSlog("StartCameraServiceProxy");  
        mSystemServiceManager.startService(CameraServiceProxy.class);  
        traceEnd();  
    }  
  
    if (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_EMBEDDED)) {  
        traceBeginAndSlog("StartIoTSystemService");  
        mSystemServiceManager.startService(IOT_SERVICE_CLASS);  
        traceEnd();  
    }  
  
    // Statsd helper  
    traceBeginAndSlog("StartStatsCompanionService");  
    mSystemServiceManager.startService(StatsCompanionService.Lifecycle.class);  
    traceEnd();  
  
    // Incidentd and dumpstated helper  
    traceBeginAndSlog("StartIncidentCompanionService");  
    mSystemServiceManager.startService(IncidentCompanionService.class);  
    traceEnd();  
  
    if (safeMode) {  
        mActivityManagerService.enterSafeMode();  
    }  
  
    // MMS service broker  
    traceBeginAndSlog("StartMmsService");  
    mmsService = mSystemServiceManager.startService(MmsServiceBroker.class);  
    traceEnd();  
  
    if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOFILL)) {  
        traceBeginAndSlog("StartAutoFillService");  
        mSystemServiceManager.startService(AUTO_FILL_MANAGER_SERVICE_CLASS);  
        traceEnd();  
    }  
  
    // NOTE: ClipboardService depends on ContentCapture and Autofill  
    traceBeginAndSlog("StartClipboardService");  
    mSystemServiceManager.startService(ClipboardService.class);  
    traceEnd();  
  
    traceBeginAndSlog("AppServiceManager");  
    mSystemServiceManager.startService(AppBindingService.Lifecycle.class);  
    traceEnd();  
  
    // It is now time to start up the app processes...  
  
    traceBeginAndSlog("MakeVibratorServiceReady");  
    try {  
        vibrator.systemReady();  
    } catch (Throwable e) {  
        reportWtf("making Vibrator Service ready", e);  
    }  
    traceEnd();  
  
    traceBeginAndSlog("MakeLockSettingsServiceReady");  
    if (lockSettings != null) {  
        try {  
            lockSettings.systemReady();  
        } catch (Throwable e) {  
            reportWtf("making Lock Settings Service ready", e);  
        }  
    }  
    traceEnd();  
  
    // Needed by DevicePolicyManager for initialization  
    traceBeginAndSlog("StartBootPhaseLockSettingsReady");  
    mSystemServiceManager.startBootPhase(SystemService.PHASE_LOCK_SETTINGS_READY);  
    traceEnd();  
  
    traceBeginAndSlog("StartBootPhaseSystemServicesReady");  
    mSystemServiceManager.startBootPhase(SystemService.PHASE_SYSTEM_SERVICES_READY);  
    traceEnd();  
  
    traceBeginAndSlog("MakeWindowManagerServiceReady");  
    try {  
        wm.systemReady();  
    } catch (Throwable e) {  
        reportWtf("making Window Manager Service ready", e);  
    }  
    traceEnd();  
  
    if (safeMode) {  
        mActivityManagerService.showSafeModeOverlay();  
    }  
  
    // Update the configuration for this context by hand, because we're going  
    // to start using it before the config change done in wm.systemReady() will    // propagate to it.    final Configuration config = wm.computeNewConfiguration(DEFAULT_DISPLAY);  
    DisplayMetrics metrics = new DisplayMetrics();  
    WindowManager w = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);  
    w.getDefaultDisplay().getMetrics(metrics);  
    context.getResources().updateConfiguration(config, metrics);  
  
    // The system context's theme may be configuration-dependent.  
    final Theme systemTheme = context.getTheme();  
    if (systemTheme.getChangingConfigurations() != 0) {  
        systemTheme.rebase();  
    }  
  
    traceBeginAndSlog("MakePowerManagerServiceReady");  
    try {  
        // TODO: use boot phase  
        mPowerManagerService.systemReady(mActivityManagerService.getAppOpsService());  
    } catch (Throwable e) {  
        reportWtf("making Power Manager Service ready", e);  
    }  
    traceEnd();  
  
    // Permission policy service  
    traceBeginAndSlog("StartPermissionPolicyService");  
    mSystemServiceManager.startService(PermissionPolicyService.class);  
    traceEnd();  
  
    traceBeginAndSlog("MakePackageManagerServiceReady");  
    mPackageManagerService.systemReady();  
    traceEnd();  
  
    traceBeginAndSlog("MakeDisplayManagerServiceReady");  
    try {  
        // TODO: use boot phase and communicate these flags some other way  
        mDisplayManagerService.systemReady(safeMode, mOnlyCore);  
    } catch (Throwable e) {  
        reportWtf("making Display Manager Service ready", e);  
    }  
    traceEnd();  
  
    mSystemServiceManager.setSafeMode(safeMode);  
  
    // Start device specific services  
    traceBeginAndSlog("StartDeviceSpecificServices");  
    final String[] classes = mSystemContext.getResources().getStringArray(  
            R.array.config_deviceSpecificSystemServices);  
    for (final String className : classes) {  
        traceBeginAndSlog("StartDeviceSpecificServices " + className);  
        try {  
            mSystemServiceManager.startService(className);  
        } catch (Throwable e) {  
            reportWtf("starting " + className, e);  
        }  
        traceEnd();  
    }  
    traceEnd();  
  
    traceBeginAndSlog("StartBootPhaseDeviceSpecificServicesReady");  
    mSystemServiceManager.startBootPhase(SystemService.PHASE_DEVICE_SPECIFIC_SERVICES_READY);  
    traceEnd();  
  
    // These are needed to propagate to the runnable below.  
    final NetworkManagementService networkManagementF = networkManagement;  
    final NetworkStatsService networkStatsF = networkStats;  
    final NetworkPolicyManagerService networkPolicyF = networkPolicy;  
    final ConnectivityService connectivityF = connectivity;  
    final LocationManagerService locationF = location;  
    final CountryDetectorService countryDetectorF = countryDetector;  
    final NetworkTimeUpdateService networkTimeUpdaterF = networkTimeUpdater;  
    final InputManagerService inputManagerF = inputManager;  
    final TelephonyRegistry telephonyRegistryF = telephonyRegistry;  
    final MediaRouterService mediaRouterF = mediaRouter;  
    final MmsServiceBroker mmsServiceF = mmsService;  
    final IpSecService ipSecServiceF = ipSecService;  
    final WindowManagerService windowManagerF = wm;  
    final CellsService cellsServiceF = cellsService;  
  
    // We now tell the activity manager it is okay to run third party  
    // code.  It will call back into us once it has gotten to the state    // where third party code can really run (but before it has actually    // started launching the initial applications), for us to complete our    // initialization.    mActivityManagerService.systemReady(() -> {  
        Slog.i(TAG, "Making services ready");  
        traceBeginAndSlog("StartActivityManagerReadyPhase");  
        mSystemServiceManager.startBootPhase(  
                SystemService.PHASE_ACTIVITY_MANAGER_READY);  
        traceEnd();  
        traceBeginAndSlog("StartObservingNativeCrashes");  
        try {  
            mActivityManagerService.startObservingNativeCrashes();  
        } catch (Throwable e) {  
            reportWtf("observing native crashes", e);  
        }  
        traceEnd();  
  
        // No dependency on Webview preparation in system server. But this should  
        // be completed before allowing 3rd party        final String WEBVIEW_PREPARATION = "WebViewFactoryPreparation";  
        Future<?> webviewPrep = null;  
        if (!mOnlyCore && mWebViewUpdateService != null) {  
            webviewPrep = SystemServerInitThreadPool.get().submit(() -> {  
                Slog.i(TAG, WEBVIEW_PREPARATION);  
                TimingsTraceLog traceLog = new TimingsTraceLog(  
                        SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);  
                traceLog.traceBegin(WEBVIEW_PREPARATION);  
                ConcurrentUtils.waitForFutureNoInterrupt(mZygotePreload, "Zygote preload");  
                mZygotePreload = null;  
                mWebViewUpdateService.prepareWebViewInSystemServer();  
                traceLog.traceEnd();  
            }, WEBVIEW_PREPARATION);  
        }  
  
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {  
            traceBeginAndSlog("StartCarServiceHelperService");  
            mSystemServiceManager.startService(CAR_SERVICE_HELPER_SERVICE_CLASS);  
            traceEnd();  
        }  
  
        traceBeginAndSlog("StartSystemUI");  
        try {  
            startSystemUi(context, windowManagerF);  
        } catch (Throwable e) {  
            reportWtf("starting System UI", e);  
        }  
        traceEnd();  
        // Enable airplane mode in safe mode. setAirplaneMode() cannot be called  
        // earlier as it sends broadcasts to other services.        // TODO: This may actually be too late if radio firmware already started leaking        // RF before the respective services start. However, fixing this requires changes        // to radio firmware and interfaces.        if (safeMode) {  
            traceBeginAndSlog("EnableAirplaneModeInSafeMode");  
            try {  
                connectivityF.setAirplaneMode(true);  
            } catch (Throwable e) {  
                reportWtf("enabling Airplane Mode during Safe Mode bootup", e);  
            }  
            traceEnd();  
        }  
        traceBeginAndSlog("MakeNetworkManagementServiceReady");  
        try {  
            if (networkManagementF != null) {  
                networkManagementF.systemReady();  
            }  
        } catch (Throwable e) {  
            reportWtf("making Network Managment Service ready", e);  
        }  
        CountDownLatch networkPolicyInitReadySignal = null;  
        if (networkPolicyF != null) {  
            networkPolicyInitReadySignal = networkPolicyF  
                    .networkScoreAndNetworkManagementServiceReady();  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeIpSecServiceReady");  
        try {  
            if (ipSecServiceF != null) {  
                ipSecServiceF.systemReady();  
            }  
        } catch (Throwable e) {  
            reportWtf("making IpSec Service ready", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeNetworkStatsServiceReady");  
        try {  
            if (networkStatsF != null) {  
                networkStatsF.systemReady();  
            }  
        } catch (Throwable e) {  
            reportWtf("making Network Stats Service ready", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeConnectivityServiceReady");  
        try {  
            if (connectivityF != null) {  
                connectivityF.systemReady();  
            }  
        } catch (Throwable e) {  
            reportWtf("making Connectivity Service ready", e);  
        }  
        traceEnd();  
        if (SystemProperties.get("ro.boot.vm","0").equals("0")) {  
        traceBeginAndSlog("MakeNetworkPolicyServiceReady");  
        try {  
            if (networkPolicyF != null) {  
                networkPolicyF.systemReady(networkPolicyInitReadySignal);  
            }  
        } catch (Throwable e) {  
            reportWtf("making Network Policy Service ready", e);  
        }  
        traceEnd();  
        }  
  
        // Wait for all packages to be prepared  
        mPackageManagerService.waitForAppDataPrepared();  
  
        // It is now okay to let the various system services start their  
        // third party code...        traceBeginAndSlog("PhaseThirdPartyAppsCanStart");  
        // confirm webview completion before starting 3rd party  
        if (webviewPrep != null) {  
            ConcurrentUtils.waitForFutureNoInterrupt(webviewPrep, WEBVIEW_PREPARATION);  
        }  
        mSystemServiceManager.startBootPhase(  
                SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);  
        traceEnd();  
  
        traceBeginAndSlog("StartNetworkStack");  
        try {  
            // Note : the network stack is creating on-demand objects that need to send  
            // broadcasts, which means it currently depends on being started after            // ActivityManagerService.mSystemReady and ActivityManagerService.mProcessesReady            // are set to true. Be careful if moving this to a different place in the            // startup sequence.            NetworkStackClient.getInstance().start(context);  
        } catch (Throwable e) {  
            reportWtf("starting Network Stack", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("MakeLocationServiceReady");  
        try {  
            if (locationF != null) {  
                locationF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying Location Service running", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeCountryDetectionServiceReady");  
        try {  
            if (countryDetectorF != null) {  
                countryDetectorF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying CountryDetectorService running", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeNetworkTimeUpdateReady");  
        try {  
            if (networkTimeUpdaterF != null) {  
                networkTimeUpdaterF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying NetworkTimeService running", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeInputManagerServiceReady");  
        try {  
            // TODO(BT) Pass parameter to input manager  
            if (inputManagerF != null) {  
                inputManagerF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying InputManagerService running", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeTelephonyRegistryReady");  
        try {  
            if (telephonyRegistryF != null) {  
                telephonyRegistryF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying TelephonyRegistry running", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeMediaRouterServiceReady");  
        try {  
            if (mediaRouterF != null) {  
                mediaRouterF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying MediaRouterService running", e);  
        }  
        traceEnd();  
        traceBeginAndSlog("MakeMmsServiceReady");  
        try {  
            if (mmsServiceF != null) {  
                mmsServiceF.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying MmsService running", e);  
        }  
        traceEnd();  
  
        traceBeginAndSlog("IncidentDaemonReady");  
        try {  
            // TODO: Switch from checkService to getService once it's always  
            // in the build and should reliably be there.            final IIncidentManager incident = IIncidentManager.Stub.asInterface(  
                    ServiceManager.getService(Context.INCIDENT_SERVICE));  
            if (incident != null) {  
                incident.systemRunning();  
            }  
        } catch (Throwable e) {  
            reportWtf("Notifying incident daemon running", e);  
        }  
        traceEnd();  
  
        try {  
            if (cellsServiceF != null) cellsServiceF.systemReady();  
        } catch (Throwable e) {  
            reportWtf("CellsService running", e);  
        }  
  
    }, BOOT_TIMINGS_TRACE_LOG);  
}
```