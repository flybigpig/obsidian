
```ad-seealso

安装win11
```

![Windows 11](https://img.sysgeek.cn/img/logo/windows/windows-10.jpg)

如果你的电脑不符合升级到 Windows 11 的[基本硬件要求](https://www.sysgeek.cn/windows-11-hardware-requirements/)，也不用担心。即使硬件配置不达标，依然可以安装 Windows 11。本文将介绍三种方法，帮助你绕过内存、CPU、TPM 和安全启动检测。

免责声明：绕过 Microsoft 的系统要求可能违反服务条款，并且可能会导致系统不稳定或安全问题。

## 方法 1：修改注册表绕过 TPM 或 CPU 检测（升级安装）

如果你的硬件不完全符合安装 Windows 11 的基本硬件要求，可以通过修改注册表，在至少拥有 TPM 1.2 和不支持的 CPU 上升级安装 Windows 11 系统：

适用场景：CPU 不在 Windows 11 支持列表中，或只有 TPM 1.2 安全芯片的 Windows 10 设备升级。

1按下`Windows + R`快捷键打开「运行」对话框，执行`regedit`打开注册表编辑器。

2导航至以下路径：

```
HKEY_LOCAL_MACHINE\SYSTEM\Setup\MoSetup
```

3在`MoSetup`注册表项下（若不存在则新建），新建一个名为`AllowUpgradesWithUnsupportedTPMOrCPU`的 **DWORD (32 位) 值**，并将其值设置为`1`。

![注册表编辑器](https://img.sysgeek.cn/img/2024/03/bypass-hardware-check-windows-11-p2.jpeg)

新建 `AllowUpgradesWithUnsupportedTPMOrCPU` 注册表项

4重启电脑后，[使用 Windows 11 安装助手](https://www.sysgeek.cn/how-to-use-windows-11-installation-assistant/)升级系统。

## 方法 2：使用 Rufus 制作无硬件要求的安装 U 盘

Rufus 是一个免费工具，可以帮助我们创建禁用 TPM、CPU 和内存检测的 Windows 11 安装 U 盘。

适用场景：适用于从 Windows 10 就地升级或全新安装 Windows 11。

1[下载 Windows 11 安装镜像（ISO）](https://www.sysgeek.cn/microsoft-windows-downloads/)。

2从 Microsoft Store 中[获取 Rufus 工具](https://apps.microsoft.com/detail/9pc3h3v7q9ch)。

3准备一个至少 16GB 的 U 盘，插入电脑，并打开 Rufus 软件。

4设置以下配置：

- **设备**：选择你的 U 盘。
- **引导类型选择**：选择下载好的 Windows 11 安装镜像（ISO）。
- **镜像选项**：选择「标准 Windows 安装」。
- **分区类型**：选择「GPT」。
- **目标系统类型**：UEFI（非 CSM）。

5点击「开始」按钮，制作 Windows 11 安装盘。

![Rufus](https://img.sysgeek.cn/img/2024/03/bypass-hardware-check-windows-11-p3.jpeg)

设置 Rufus

6在弹出窗口中，勾选以下选项：

- 移除对 4GB+ 内存、安全引导和 TPM 2.0 的要求
- 移除对登录微软账户的要求（可选）

![Rufus](https://img.sysgeek.cn/img/2024/03/bypass-hardware-check-windows-11-p4.jpeg)

Rufus：移除 Windows 11 硬件要求

7点击「OK」，然后点击「确定」以清除 U 盘。

![Rufus](https://img.sysgeek.cn/img/2024/03/bypass-hardware-check-windows-11-p5.jpeg)

确定清除并写入 U 盘

8U 盘写入完成后，就可以拿去直接使用了。

## 方法 3：在安装时绕过 TPM、安全启动和内存检测

如果你有写好的 Windows 11 安装 U 盘，也可以在安装过程中更改注册表来绕过 TPM、安全启动和内存检测：

适用场景: 绕过 TPM、安全启动和内存检测，全新安装 Windows 11，但无法绕过双核 CPU 要求。

1从你的 Windows 11 安装 U 盘启动。

2按下`Shift + F10`键打开「命令提示符」，执行`regedit`打开注册表编辑器。

3导航至以下路径：

```
HKEY_LOCAL_MACHINE\SYSTEM\Setup
```

4创建一个名为`LabConfig`的注册表键，然后创建以下 3 项目 **DWORD（32 位）值**，并将其值设置为`1`：

- `BypassTPMCheck`
- `BypassSecureBootCheck`
- `BypassRAMCheck`

![注册表编辑器](https://img.sysgeek.cn/img/2024/03/bypass-hardware-check-windows-11-p6.jpeg)

安装时绕过 Windows 11 硬件检测

5关闭注册表编辑器和命令提示符，按照正常流程继续安装 Windows 11 系统。


