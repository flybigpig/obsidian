/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

package com.demo.nativeservice.client;

import android.app.Activity;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.demo.nativeservice.IDemoService;

public class MainActivity extends Activity {
    private static final String TAG = "DemoServiceClient";
    private IDemoService mService;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Button btnAdd = new Button(this);
        btnAdd.setText("Call add()");
        btnAdd.setOnClickListener(v -> callAdd());

        Button btnName = new Button(this);
        btnName.setText("Get Name");
        btnName.setOnClickListener(v -> callGetName());

        setContentView(btnAdd);
    }

    @Override
    protected void onResume() {
        super.onResume();
        connectService();
    }

    private void connectService() {
        if (mService != null) return;
        IBinder binder = ServiceManager.getService("demo_service");
        if (binder != null) {
            mService = IDemoService.Stub.asInterface(binder);
            Log.i(TAG, "Connected to demo_service");
        } else {
            Log.e(TAG, "demo_service not found");
        }
    }

    private void callAdd() {
        if (mService == null) {
            Toast.makeText(this, "Service not connected", Toast.LENGTH_SHORT).show();
            return;
        }
        try {
            int result = mService.add(10, 20);
            Toast.makeText(this, "10 + 20 = " + result, Toast.LENGTH_SHORT).show();
            Log.i(TAG, "add(10, 20) = " + result);
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException: " + e.getMessage());
        }
    }

    private void callGetName() {
        if (mService == null) {
            Toast.makeText(this, "Service not connected", Toast.LENGTH_SHORT).show();
            return;
        }
        try {
            String name = mService.getName();
            Toast.makeText(this, "Name: " + name, Toast.LENGTH_SHORT).show();
            Log.i(TAG, "getName() = " + name);
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException: " + e.getMessage());
        }
    }
}
