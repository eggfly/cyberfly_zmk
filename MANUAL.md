---
name: CyberFly BLE / USB Dual-mode Keyboard 使用说明
date: 2026-05-07
firmware-source: cyberfly_zmk/app/boards/shields/cyberfly_keyboard/
---

# ⌨️ CyberFly 键盘使用说明书

> *小小一块板，BLE + USB 双模、背光、飞鼠、6 台主机切换，怎么折腾都行。*

CyberFly 是一款基于 **nRF52840**（Nice!Nano v2 引脚兼容）+ **ZMK** 固件的 BLE + USB 双模机械键盘。板载 **QMI8658A 6 轴 IMU**，可作空间飞鼠（固件调优中）。BLE 下 macOS / iOS 原生读取电量（系统"我的设备"卡片、桌面电池小组件）。

<p align="center">
  <img src="images/CyberFly_Render.png" alt="CyberFly 键盘渲染图" width="600"/>
</p>

## 🪪 设备身份

| 项目 | 值 |
|---|---|
| USB HID | VID `0x1209` / PID `0x0001`，`CyberFly Keyboard` |
| BLE 设备名 | `CyberFly KBD` |
| 最大配对数 | **6 台主机** |
| 物理有效键位 | **69 键**（空格按 1 键）/ **71 键**（空格左/右分独立）|
| 矩阵 | 6 行 × 13 列（C12 飞线）|

## 💡 指示灯一览

| 位置 | 颜色 | 含义 |
|---|---|---|
| 键盘板**上方** | 🌈 **RGB 灯** | 工作状态（开机动画、USB 循环、BLE 心跳、飞鼠切换）|
| RGB 旁边 | 🔴 **红灯** | 充电中常亮，充满自动熄灭 |
| 键盘板**下方** | ⚪ **白灯** | USB 5V 供电指示，插线就亮 |

### RGB 状态详解

| 场景 | 效果 |
|---|---|
| 🚀 开机 | 红 → 绿 → 蓝 顺序闪一次然后熄灭（启动动画）|
| 🔌 USB 已连接 | 持续循环变换（红绿蓝三色轮换）|
| 🔋 BLE 待机 | **蓝色双闪**，每 5 秒一次心跳，代表随时可用 |
| 🖱️ 飞鼠切换 | OFF=红闪 / M1=绿闪 / M2=蓝闪（双闪后回归原状态）|

## 🎮 工作模式

| 模式 | 连接 | 背光 | 飞鼠 |
|---|---|---|---|
| 🔌 **USB** | USB-C 线 | ✅ 可用（Fn+空格调节）| ✅ 可用 |
| 📡 **BLE** | 蓝牙 | ❌ 自动关闭（省电）| ⚠️ 默认禁用（需手动开启）|

> 🔋 背光**仅 USB 通电时可用**，拔线自动熄灭。
> 配置：`AUTO_OFF_USB=y`（USB 拔掉即灭）、`AUTO_OFF_IDLE=y`（空闲自动灭）。

## ⌨️ 键位布局

实际物理键帽以 PCB 丝印为准（**不是**矩阵扫描顺序）：

| ESC | □ |   | △ |   | ✕ |   | ○ |   | ☁ |   | ◇ | BSP |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ~ | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | - | = |
| TAB | Q | W | E | R | T | Y | U | I | O | P | [ | ] |
| Fn | A | S | D | F | G | H | J | K | L | ; | ' | ↵ |
| ⇧ | Z | X | C | V | B | N | M | , | . | / | ↑ | ⇧ |
| CTL | ⌘ | ALT | \ | SPC |   | SPC |   | SPC | ALT | ← | ↓ | → |

> 💡 Row 0 中间是 MCU 区域没有按键。电池：**LP 301060**（约 250mAh）。

**制表符版本（等宽字体下查看）：**

```
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ESC│ □ │   │ △ │   │ ✕ │   │ ○ │   │ ☁ │   │ ◇ │BSP│
├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
│ ~ │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ 0 │ - │ = │
├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
│TAB│ Q │ W │ E │ R │ T │ Y │ U │ I │ O │ P │ [ │ ] │
├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
│ Fn│ A │ S │ D │ F │ G │ H │ J │ K │ L │ ; │ ' │ ↵ │
├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
│ ⇧ │ Z │ X │ C │ V │ B │ N │ M │ , │ . │ / │ ↑ │ ⇧ │
├───┼───┼───┼───┼───┴───┴───┴───┴───┼───┼───┼───┼───┤
│CTL│ ⌘ │ALT│ \ │       SPACE       │ALT│ ← │ ↓ │ → │
└───┴───┴───┴───┴───────────────────┴───┴───┴───┴───┘
```

**键位彩蛋：**

- 🎯 **Fn** 在左侧第 4 行起始位（CapsLock 位置），按住激活 Fn 层
- 🔤 **Esc 双击** → caps_word（一次性大写，遇空格自动退出）
- ⇧ **左 Shift 双击** → 锁 CapsLock
- 🧭 **大空格键**：物理上一整条，矩阵占 3 列（左/中/右）；飞鼠开启后左右区域变**鼠标左/右键**（smart_space）

### 🔗 Combo 组合键

| 同按 | 触发 | 判定窗口 |
|---|---|---|
| **J + K** | Esc | 50 ms |
| **D + F** | Tab | 50 ms |
| **J + K + L** | CapsWord（一次性大写）| 50 ms |
| **F + J** | Enter | 50 ms |
| **Fn + RShift** | 🖱️ 开/关飞鼠（OFF → M1 → M2 循环）| 75 ms |

### 📜 Macro 宏

| 键 | 宏效果 |
|---|---|
| Fn + Q | 📸 截屏（⌘ + Shift + 4）|
| Fn + A | 📋 全选 + 复制（⌘A → 50ms → ⌘C）|
| Fn + M | ➜ 箭头符号 `() => {}` |

## 🎹 Fn 层完整映射

按住 **Fn**（左侧 CapsLock 位）后键位变化：

| 原键 | Fn + 后 |
|---|---|
| **B** | 🚪 进入 Bootloader（UF2 刷机模式）|
| **C** | 🧹 Clean Settings（清 NVS → 自动 warm reboot）|
| 空格 × 3 | 🌈 循环切换背光（`BL_CYCLE`）|
| Esc | 🔄 切换 USB / BLE 输出（`OUT_TOG`）|
| F1 ~ F6 | 📡 切蓝牙设备 0 ~ 5（`BT_SEL 0..5`）|
| Enter | 🗑️ 清除**当前通道**配对（`BT_CLR`）|
| BSPC | DEL |
| `-` / `=` | F11 / F12 |
| 1 ~ 0 | F1 ~ F10 |
| Q | 截屏宏 |
| A | 全选复制宏 |
| M | `() => {}` 宏 |
| ] | ⚡ 外部电源开关（`EP_TOG`）|
| Up / Down / Left / Right | PgUp / PgDn / Home / End |
| LAlt / RAlt | 🖱️ 开/关飞鼠（同 Fn + RShift）|
| RShift（单按）| 💤 软关机（`soft_off`）|

> 🚪 `Fn + B` 让芯片跳 UF2 bootloader，会出现 `NICENANO` 虚拟 U 盘
> 🧹 `Fn + C` 只清 ZMK 设置并软重启（**不擦**固件）
> 💤 `Fn + RShift` **75ms 内同按**触发飞鼠切换；**分前后按**会走 Fn 层的 `soft_off`。想关机就慢点按，想切飞鼠就同时按下。

## 🖱️ 6 轴飞鼠（Air Mouse）

板载 **QMI8658A** 6 轴 IMU（加速度计 + 陀螺仪，I²C 总线）。

### 三档模式（循环切换）

| 模式 | 算法 | 状态灯 | 风格 |
|---|---|---|---|
| **OFF** | 关闭，IMU 进省电 | 🔴 红闪 | 纯键盘 |
| **M1** | Kalman 滤波 + 弹簧阻尼补偿 | 🟢 绿闪 | 稳 |
| **M2** | 加速度映射 | 🔵 蓝闪 | 跟手 |

**三种切换方式：**

- 🤝 `Fn + RShift`（combo，75ms 内同按）
- 🎯 `Fn + LAlt` 或 `Fn + RAlt`（进入 Fn 层后按）

切换时 RGB 对应颜色双闪一下确认。

### Smart Space：飞鼠开启后大空格键的魔法

| 位置 | OFF 状态 | ON 状态（M1/M2）|
|---|---|---|
| 左空格 | 空格 | 🖱️ **鼠标左键** |
| 中空格 | 空格 | 空格（保持）|
| 右空格 | 空格 | 🖱️ **鼠标右键** |

> ⚠️ **飞鼠仍在调优**（漂移滤波、加速度曲线），稳定版之前仅作体验。

## 📡 蓝牙 / USB 切换

- 🔄 **Fn + Esc** 切 USB / BLE 输出
- 📱 **Fn + F1 ~ F6** 切换蓝牙通道 0 ~ 5（同时保持 6 台配对）
- 🗑️ **Fn + Enter** 清**当前通道**配对
- 💣 **Fn + C** 清**所有**设置（含全部通道）

### 配对流程

1. 新键盘首次开机自动广播
2. 主机蓝牙里搜 `CyberFly KBD`，点配对
3. 连接后系统"我的设备"里显示电量（如：`已连接 - 🔋 100%`）
4. 想加第二台：`Fn + F2` → 配对 → 以此类推至 `Fn + F6`
5. 配对抽风？`Fn + C` 清 NVS 重来

### 电量显示

- 🍎 **macOS**: 系统设置 → 蓝牙 → "我的设备"卡片
- 📱 **iOS / iPadOS**: 桌面加 "电池" 小组件
- 📏 协议：标准 **BLE Battery Service (0x180F)** → Battery Level (0x2A19)


## 🔋 待机 / 续航

- ⏳ BLE 待机 **> 48 小时**
- 🛌 长期不用：**拨电源开关到 OFF** 物理断电
- 💾 **配对信息不会丢失**（存 flash NVS，不靠电池）

## 🔌 电源 / 充电 / 开关

- 🪫 充电口：**USB-C**
- 🔴 充电指示：红灯 → 充电中常亮，满电自动熄灭
- ⚪ USB 5V 供电：板下白灯，插线就亮
- 🔘 硬件电源拨动开关
  - **ON** — 电池供电，BLE 正常
  - **OFF** — 物理切断电池，**零功耗长期存放**


## 🛠️ Bootloader / 固件升级

### 进入 Bootloader 的两种方式

1. ⌨️ **Fn + B**（正常工作时）
2. 🔁 **板载 reset 按钮双击**（固件挂了或 Fn+B 失效时）

进 bootloader 后：

- 💡 板子主 LED 慢速呼吸（Adafruit UF2 0.10.0 指示）
- 💾 出现名为 `NICENANO` 的 FAT16 虚拟 U 盘
- 🎯 把 `.uf2` 文件拖进去，写完自动重启到新固件

### 本地编译固件

```bash
cd cyberfly_zmk
pip3 install west
west init -l app
west update
west build -s app -b nice_nano_nrf52840_zmk -- -DSHIELD=cyberfly_keyboard
# 产物: build/zephyr/zmk.uf2
```

### 固件栈

- 🚪 **Bootloader**: Adafruit nRF52 UF2 **v0.10.0**（nice!nano fork）
- 📡 **SoftDevice**: Nordic **S140 v6.1.1**（BLE 协议栈）
- 🧠 **应用层**: ZMK 定制分支 `cyberfly_zmk`
  - Pointing (mouse keys)、Studio-ready、Smart Space、Toggle Mouse、RGB Status LED 等自定义 behavior


## ❓ 常见问题（FAQ）

**Q: 为什么背光不亮？**
A: 只有 USB 通电时才亮，BLE 模式下为省电全关。`Fn + 空格` 循环切换亮度。

**Q: 电量显示不准？**
A: 首次连接等 1 分钟左右让 BLE 电池服务同步；低于 20% 时 iOS 桌面小组件会红色图标。

**Q: Fn + B 后没出现 NICENANO 盘？**
A: USB 线可能只通电不走数据，换一根 data 线。

**Q: 我的 BLE 主机连不上？**
A: 按顺序试：
1. `Fn + Enter` 清当前通道
2. `Fn + C` 清所有设置
3. 主机蓝牙设置里"忘记此设备"再配

**Q: 按 Fn + RShift 有时出软关机有时出飞鼠？**
A: 75ms **同按**触发飞鼠切换（combo），**分前后按**会走 Fn 层的 `soft_off`。想关机按住 Fn 再慢点按 RShift；想切飞鼠就同时按下。

**Q: 怎么从 OFF 状态快速回到 M2 模式？**
A: `Fn + RShift` 按两下（OFF → M1 → M2）。

**Q: 长期不用怎么处理？**
A: 硬件开关拨 OFF，配对信息保留，下次回 ON 自动回连最近主机。


## 📋 硬件参数摘要

| 项目 | 值 |
|---|---|
| 主控 | nRF52840（Nice!Nano v2 兼容）|
| 无线 | BLE 5.0（S140 v6.1.1）+ USB HID |
| 矩阵 | 6 行 × 13 列（C12 飞线）|
| 去抖 | 按下 10ms / 释放 10ms |
| IMU | QMI8658A 6 轴（I²C）|
| 外部 I²C / Qwiic | **JST-SH 1.0mm 4-pin**（SM04B-SRSS, C160404）<br>引脚：GND / 3V3 / SDA / SCL，兼容 SparkFun Qwiic / Adafruit STEMMA QT 生态<br>可直接挂 OLED、传感器等；10K 上拉到 3.3V 默认 DNP |
| 背光 | PWM 驱动 AP3032 升压 LED driver @ P0.15 |
| RGB 状态灯 | PWM1 × 3 通道：R=P0.14 / G=P0.16 / B=P0.19 |
| 外部电源控制 | P0.13 → LDO CE |
| 充电 | USB-C，板载管理，红灯指示 |
| 最大配对 | 6 台主机 |
| 电池 | LP 301060 锂聚合物（约 250 mAh）|
| 电源开关 | 硬件拨动（PCB 左上角丝印 ON），OFF = 物理断电池 |


## 📂 版本 & 源码

本说明书基于仓库当前状态生成，源码路径：

- 📦 `app/boards/shields/cyberfly_keyboard/` — **完整版**（飞鼠 + 自定义 behavior + RGB 状态灯）
- 🍼 `app/boards/shields/cyberfly/` — 简化版（仅键位，CI 默认编译，便于快速上手）

> 🔍 如果你拿到的是 **飞鼠 + 状态灯 + smart space** 版本，对应 shield 是 `cyberfly_keyboard`。

<p align="center">
  <i>Happy clacking! ⌨️✨</i>
</p>
