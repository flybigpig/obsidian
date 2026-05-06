package com.example.binder

import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.example.binder.databinding.ActivityMainBinding

/**
 * 主界面 - 演示 Binder 客户端调用
 */
class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private lateinit var binderClient: BinderClient

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // 初始化 Binder 客户端
        binderClient = BinderClient(this)

        setupButtons()
    }

    override fun onStart() {
        super.onStart()
        // 绑定服务
        binderClient.bindService()
    }

    override fun onStop() {
        super.onStop()
        // 解绑服务
        binderClient.unbindService()
    }

    private fun setupButtons() {
        // 获取服务名称
        binding.btnGetName.setOnClickListener {
            val name = binderClient.getServiceName()
            binding.tvResult.text = "Service Name: $name"
        }

        // 计算加法
        binding.btnAdd.setOnClickListener {
            val result = binderClient.add(100, 200)
            binding.tvResult.text = "100 + 200 = $result"
        }

        // 发送消息
        binding.btnSendMsg.setOnClickListener {
            binderClient.sendMessage("Hello from client!")
            binding.tvResult.text = "Message sent!"
        }

        // 检查连接
        binding.btnCheckConn.setOnClickListener {
            val connected = binderClient.isConnected()
            binding.tvResult.text = "Connected: $connected"
        }
    }
}
