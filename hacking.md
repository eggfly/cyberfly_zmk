# CyberFly ZMK - 开发与刷机指南

## 环境版本

| 组件 | 版本 |
|------|------|
| ZMK | main branch (基于 zmkfirmware/zmk) |
| Zephyr | 4.1.0 (`v4.1.0+zmk-fixes`) |
| Zephyr SDK | 0.16.9 |
| ARM GCC | arm-zephyr-eabi-gcc 12.2.0 (Zephyr SDK 0.16.9) |
| Python | 3.10+ (推荐 3.12) |
| west | 1.5.0 |
| CMake | 3.20+ |
| Ninja | 需要安装 |
| dtc | 1.4.6+ |

## 硬件

- 开发板: ProMicro NRF52840 兼容板 (已刷 Nice!Nano bootloader)
- MCU: nRF52840 (ARM Cortex-M4F, 256KB RAM, 1MB Flash)
- 接口: USB Type-C
- 键盘矩阵板: CyberFly_Keyboard_6R13C (6 行 x 13 列 = 78 键位, ROW2COL)

## nRF52840 Flash 布局

```
地址          内容                        大小
─────────────────────────────────────────────────
0x000000      MBR (Master Boot Record)    4KB
0x001000      SoftDevice S140 v6.1.1      ~148KB     ← BLE 协议栈
0x026000      Application (ZMK 固件)      ~760KB     ← zmk.uf2 写入地址
0x0EC000      Storage (NVS)               32KB       ← 蓝牙配对、设置存储
0x0F4000      UF2 Bootloader              48KB       ← BPROT 保护，不可覆盖
0x100000      Flash 结束                  共 1MB
```

### 各部分说明

- **MBR**: Nordic 的 Master Boot Record，引导链的起点，跳转到 SoftDevice
- **SoftDevice S140**: Nordic 的闭源 BLE 协议栈，提供蓝牙功能。ZMK 的蓝牙依赖它
- **Application**: 你的键盘固件。`zmk.uf2` 只包含这个区域
- **Storage (NVS)**: Non-Volatile Storage，存储蓝牙配对信息、键盘设置等
- **UF2 Bootloader**: 基于 Adafruit nRF52 Bootloader，提供 USB Mass Storage 刷机功能

### UF2 Bootloader 工作原理

双击 Reset 进入 Bootloader 模式的流程：

1. **第一次 Reset** → Bootloader 在 RAM 中写入 magic value（RAM 在 reset 时不被清零）
2. **第二次 Reset**（约 0.5s 内）→ Bootloader 检测到 magic value → 进入 USB Mass Storage 模式
3. **如果没有第二次 Reset** → magic value 超时失效 → 正常启动 Application

进入 Bootloader 后：
- 呈现一个**虚拟 FAT12 文件系统**（`NICENANO` U盘）
- `CURRENT.UF2` — Bootloader 实时读取 flash，包装成 UF2 格式（SoftDevice + App 的完整备份）
- `INFO_UF2.TXT` — Bootloader 版本信息
- 拷入 `.uf2` 文件 → Bootloader 解析 UF2 块 → 写入对应 flash 地址 → 自动 Reset

### CURRENT.UF2 vs zmk.uf2

| | CURRENT.UF2 | zmk.uf2 |
|---|---|---|
| 内容 | flash 完整转储 (SoftDevice + App) | 仅应用固件 |
| 起始地址 | 0x001000 | 0x026000 |
| family ID | 0x239a00b3 (Adafruit nRF52) | 0xADA52840 (Nordic nRF52840) |
| 用途 | 备份当前固件 | 刷入新固件 |

## 本地构建环境搭建 (macOS Apple Silicon)

```bash
# 1. 安装基础工具
brew install cmake ninja dtc wget

# 2. 创建 Python venv 并安装 west
python3.12 -m venv .venv
source .venv/bin/activate
pip install west

# 3. 初始化 workspace 并下载依赖
west init -l app
west update
west zephyr-export

# 4. 安装 Zephyr Python 依赖
pip install -r zephyr/scripts/requirements.txt

# 5. 安装 Zephyr SDK 0.16.9
#    下载 minimal SDK
curl -L "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.9/zephyr-sdk-0.16.9_macos-aarch64_minimal.tar.xz" -o /tmp/zephyr-sdk.tar.xz
mkdir -p ~/zephyr-sdk
tar xf /tmp/zephyr-sdk.tar.xz -C ~/zephyr-sdk/ --strip-components=1

#    下载 ARM 工具链
curl -L "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.9/toolchain_macos-aarch64_arm-zephyr-eabi.tar.xz" -o /tmp/arm-tc.tar.xz
tar xf /tmp/arm-tc.tar.xz -C ~/zephyr-sdk/
```

## 构建固件

```bash
source .venv/bin/activate
export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk

# 清除旧构建并编译
rm -rf build
west build -s app -b nice_nano/nrf52840/zmk -- -DSHIELD=cyberfly

# 固件产出
# build/zephyr/zmk.uf2
```

## USB 刷机步骤

1. 用 USB-C 线连接开发板到电脑

2. 进入 Bootloader 模式:
   - 用导线/镊子**快速短接两次** RST 焊盘和 GND（间隔约 0.5 秒）
   - 成功后电脑会出现 `NICENANO` U 盘

3. 复制固件:
   ```bash
   # macOS
   cp build/zephyr/zmk.uf2 /Volumes/NICENANO/

   # Linux
   cp build/zephyr/zmk.uf2 /run/media/$USER/NICENANO/
   ```

4. 复制完成后开发板自动重启，`NICENANO` U 盘消失即表示刷入成功

## 验证

刷入后:
- **USB**: 插着 USB 线会被识别为 HID 键盘，设备名 "CyberFly"，厂商 "eggfly"
- **蓝牙**: 广播名 "CyberFly"，在系统蓝牙设置中搜索配对

## 文件结构

```
app/boards/shields/cyberfly/
├── Kconfig.defconfig        # ZMK 键盘名 "CyberFly"
├── Kconfig.shield           # Shield 构建定义
├── cyberfly.conf      # 启用 BLE/USB, USB 描述符
├── cyberfly.keymap    # 键位映射 (3层: Default, Fn, BT)
├── cyberfly.overlay   # 设备树: 6x13 矩阵 GPIO 引脚定义
├── cyberfly.zmk.yml   # Shield 元数据
└── README.md                # 详细说明 (引脚接线表等)
```

## 引脚分配 (ProMicro NRF52840 → CyberFly 6R13C)

6 行 + 13 列 = 19 引脚。ProMicro 标准接口只有 18 个 GPIO，第 13 列需要飞线到 P1.02。

| 功能 | Pro Micro 编号 | nRF52840 GPIO | 矩阵板接口 |
|------|---------------|---------------|-----------|
| ROW0 | D0 | P0.08 | CN1.1 |
| ROW1 | D1 | P0.06 | CN1.2 |
| ROW2 | D2 | P0.17 | CN1.3 |
| ROW3 | D3 | P0.20 | CN1.4 |
| ROW4 | D4 | P0.22 | CN1.5 |
| ROW5 | D5 | P0.24 | CN1.6 |
| COL0 | D6 | P1.00 | H1.1 |
| COL1 | D7 | P0.11 | H1.2 |
| COL2 | D8 | P1.04 | H1.3 |
| COL3 | D9 | P1.06 | H1.4 |
| COL4 | D10 | P0.09 | H1.5 |
| COL5 | D16 | P0.10 | H1.6 |
| COL6 | D14 | P1.11 | H1.7 |
| COL7 | D15 | P1.13 | H1.8 |
| COL8 | D18 | P1.15 | H1.9 |
| COL9 | D19 | P0.02 | H1.10 |
| COL10 | D20 | P0.29 | H1.11 |
| COL11 | D21 | P0.31 | H1.12 |
| COL12 | **飞线** | **P1.02** | H1.13 |

## 开发板 + 测试矩阵板接线指南

### 你需要的东西

- ProMicro NRF52840 开发板 (已刷 Nice!Nano bootloader)
- CyberFly_Keyboard_6R13C 矩阵测试板
- 杜邦线 19 根 (母对母或根据排针类型选择)
- USB-C 数据线

### 矩阵板接口说明

矩阵板有两个排针座：

```
┌─ CyberFly_Keyboard_6R13C 矩阵板 (正面朝上) ─┐
│                                               │
│  H1 (13pin 列排针，板子顶部)                    │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐ │
│  │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │ │
│  │C0 │C1 │C2 │C3 │C4 │C5 │C6 │C7 │C8 │C9 │C10│C11│C12│ │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘ │
│                                               │
│           [ 键盘矩阵按键区域 ]                    │
│                                               │
│  CN1 (6pin 行排针，板子左侧或底部)               │
│  ┌───┐                                        │
│  │ 1 │ ROW0                                   │
│  │ 2 │ ROW1                                   │
│  │ 3 │ ROW2                                   │
│  │ 4 │ ROW3                                   │
│  │ 5 │ ROW4                                   │
│  │ 6 │ ROW5                                   │
│  └───┘                                        │
│                                               │
│  H2 (2pin LED 供电，可不接)                      │
│  ┌───┬───┐                                    │
│  │ 1 │ 2 │  1=GND, 2=LED+                     │
│  └───┴───┘                                    │
└───────────────────────────────────────────────┘
```

### ProMicro NRF52840 引脚布局

```
          USB-C
      ┌─────────┐
      │  ┌───┐  │
      │  │USB│  │
      │  └───┘  │
  D0  │●       ●│ RAW
  GND │●       ●│ GND
  GND │●       ●│ RST
  VCC │●       ●│ VCC
  D1  │●       ●│ D21
  D2  │●       ●│ D20
  D3  │●       ●│ D19
  D4  │●       ●│ D18
  D5  │●       ●│ D15
  D6  │●       ●│ D14
  D7  │●       ●│ D16
  D8  │●       ●│ D10
  D9  │●       ●│ D9  (注意: 左右两边都标 D9)
      └─────────┘
      (底部中间有 P1.02 等额外焊盘)
```

> **注意**: 上面引脚布局仅供参考，请以你实际开发板背面的丝印标注为准！不同厂家的克隆板引脚标注可能不同。

### 接线步骤

#### 第一步: 接 6 根行线 (ROW，开发板左侧)

用 6 根杜邦线连接：

```
ProMicro 左侧          矩阵板 CN1
─────────────          ──────────
D0  (P0.08)  ────────  Pin 1 (ROW0)
D1  (P0.06)  ────────  Pin 2 (ROW1)
D2  (P0.17)  ────────  Pin 3 (ROW2)
D3  (P0.20)  ────────  Pin 4 (ROW3)
D4  (P0.22)  ────────  Pin 5 (ROW4)
D5  (P0.24)  ────────  Pin 6 (ROW5)
```

#### 第二步: 接 12 根列线 (COL0-COL11，开发板两侧)

```
ProMicro 引脚           矩阵板 H1
──────────────          ──────────
D6  (P1.00) 左侧 ────  Pin 1  (COL0)
D7  (P0.11) 左侧 ────  Pin 2  (COL1)
D8  (P1.04) 左侧 ────  Pin 3  (COL2)
D9  (P1.06) 左侧 ────  Pin 4  (COL3)
D10 (P0.09) 右侧 ────  Pin 5  (COL4)
D16 (P0.10) 右侧 ────  Pin 6  (COL5)
D14 (P1.11) 右侧 ────  Pin 7  (COL6)
D15 (P1.13) 右侧 ────  Pin 8  (COL7)
D18 (P1.15) 右侧 ────  Pin 9  (COL8)
D19 (P0.02) 右侧 ────  Pin 10 (COL9)
D20 (P0.29) 右侧 ────  Pin 11 (COL10)
D21 (P0.31) 右侧 ────  Pin 12 (COL11)
```

#### 第三步: 飞线接第 13 列 (COL12)

ProMicro 标准引脚已经用完，COL12 需要焊接飞线：

```
ProMicro 板子底部/背面
P1.02 焊盘  ──────────  矩阵板 H1 Pin 13 (COL12)
```

> **P1.02 焊盘位置**: 在 ProMicro NRF52840 背面中间区域，丝印标注 "1.02" 或 "102"。
> 需要用烙铁焊一根细线到这个焊盘，另一端接杜邦线头。

#### GND 不需要单独接

矩阵板的行列线不需要 GND 连接（矩阵扫描通过 GPIO 高低电平完成）。
H2 的 GND/LED 是给背光 LED 用的，测试阶段可以不接。

### 接线检查清单

- [ ] 共 19 根线: 6 根行线 + 12 根列线 + 1 根飞线
- [ ] 确认没有短路 (特别是相邻引脚)
- [ ] USB-C 线连接到开发板 (供电 + 通信)
- [ ] 开发板已刷入 cyberfly ZMK 固件

### 快速验证

接好线后：
1. USB 插上开发板，macOS 应识别为 "CyberFly" 键盘
2. 打开文本编辑器
3. 用手指同时按矩阵板上的一个按键触点的行和列 (或用镊子短接)
4. 应该能看到对应字符输出
5. 如果没反应，检查对应行列线是否接对

---

## 自制 nRF52840 键盘 PCB 指南

### 全新 nRF52840 芯片出厂状态

**全新的 nRF52840 芯片 Flash 是全空的 (0xFF)**，没有任何固件：
- 没有 Bootloader
- 没有 SoftDevice
- 没有 MBR
- FICR (Factory Information Configuration Registers) 区域有 Nordic 出厂写入的芯片唯一 ID、校准数据等（只读，不可擦除）

也就是说，你自己做 PCB 焊好芯片后，**必须通过 SWD 接口首次烧录**，无法通过 USB 刷机（因为 USB 功能需要 Bootloader 提供）。

### 首次烧录流程

```
全新芯片 (Flash 空白)
    │
    ▼  通过 SWD (J-Link / DAPLink / ST-Link)
    │
    ├── 1. 烧录 SoftDevice S140 v6.1.1
    ├── 2. 烧录 Nice!Nano Bootloader
    │       (nice_nano_bootloader-0.6.0_s140_6.1.1.hex，包含 MBR)
    │
    ▼  之后可通过 UF2 U盘模式
    │
    └── 3. 拷贝 zmk.uf2 刷入应用固件
```

### 所需硬件工具

| 工具 | 用途 | 价格参考 |
|------|------|---------|
| **J-Link** (推荐) | SWD 调试器，首次烧录 Bootloader | 正版贵，淘宝 J-Link OB 约 20-50 元 |
| **DAPLink** (替代) | 开源 SWD 调试器，便宜 | 约 10-30 元 |
| **nRF Command Line Tools** | Nordic 官方命令行烧录工具 (nrfjprog) | 免费 |
| **nRF Connect for Desktop** | Nordic 官方 GUI 烧录工具 (Programmer) | 免费 |

### SWD 接线 (仅需 4 根线)

```
J-Link / DAPLink          nRF52840 PCB
──────────────────────────────────────
  VCC ──────────────────── VDD (3.3V)
  GND ──────────────────── GND
  SWDIO ────────────────── SWDIO (P0.18，芯片固定引脚)
  SWDCLK ───────────────── SWDCLK (P0.20? 不对，是专用引脚)
```

> **注意**: SWDIO 和 SWDCLK 是 nRF52840 的专用调试引脚，不是 GPIO，在芯片的固定 pad 上。参考 nRF52840 数据手册引脚图。

### 烧录命令 (使用 nrfjprog + J-Link)

```bash
# 1. 擦除芯片
nrfjprog --family NRF52 --eraseall

# 2. 烧录 SoftDevice
nrfjprog --family NRF52 --program s140_nrf52_6.1.1_softdevice.hex --sectorerase

# 3. 烧录 Nice!Nano Bootloader (已包含 MBR)
nrfjprog --family NRF52 --program nice_nano_bootloader-0.6.0_s140_6.1.1.hex --sectorerase

# 4. Reset 启动
nrfjprog --family NRF52 --reset
```

> Bootloader hex 文件在本仓库 `SuperMini NRF52840 资料20230808/BootLoader/` 目录下。

烧录完成后，双击 Reset 就能看到 `NICENANO` U盘，然后通过 UF2 刷 ZMK 固件。

### 自制 PCB Checklist

#### 必须有的

- [ ] **nRF52840 芯片** (QFN48 封装，推荐 QIAA variant，1MB Flash / 256KB RAM)
- [ ] **32.768 kHz 低频晶振** — 蓝牙协议栈必需，精度要求 ±20ppm
- [ ] **32 MHz 高频晶振** — 射频必需，精度要求 ±40ppm
- [ ] **射频匹配电路** — 按 Nordic 参考设计，包含 π 型匹配网络 + 巴伦
- [ ] **天线** — PCB 天线 / 陶瓷天线 / IPEX 座 + 外置天线
- [ ] **去耦电容** — VDD 引脚需要 100nF 去耦，DEC1-DEC6 引脚按数据手册要求
- [ ] **USB Type-C 接口** — 数据线接 D+ (P0.13) / D- (P0.15/0.18... 看数据手册)
- [ ] **SWD 调试接口** — 至少预留 SWDIO + SWDCLK + GND + VCC 四个测试点或焊盘
- [ ] **Reset 按钮** — 连接 nRESET 引脚到 GND（建议加物理按钮，方便进 Bootloader）
- [ ] **电源**: 3.3V LDO 或 DC-DC（nRF52840 支持 1.7-5.5V，推荐用板载 DC-DC）

#### 推荐有的

- [ ] **电池接口** — 如果要电池供电，加 JST 座 + 充电 IC (如 MCP73831)
- [ ] **电池电压检测** — 分压电阻接一个 ADC 引脚，监测电量
- [ ] **LED 指示灯** — 至少一个 GPIO 控制的 LED 用于状态指示
- [ ] **ESD 保护** — USB 数据线加 ESD 保护二极管
- [ ] **用户按钮** — 除了 Reset 外，可加一个 GPIO 按钮方便调试

#### 键盘矩阵相关

- [ ] **GPIO 分配** — 6 行 + 13 列 = 19 个 GPIO，nRF52840 有 48 个 GPIO 足够用
- [ ] **二极管** — 如果要防鬼键 (anti-ghosting)，每个按键加 1N4148 二极管
- [ ] **矩阵走线** — 行列线尽量不要跨层或平行走太长，减少串扰

#### 射频注意事项 (重要!)

- [ ] 天线匹配电路严格按 Nordic 参考设计，**不要自己改值**
- [ ] 天线附近 PCB **不铺铜**（保持净空区）
- [ ] 射频走线做 **50Ω 阻抗控制**
- [ ] 天线尽量放在 PCB 边缘，远离金属件和电池
- [ ] 如果不确定射频设计，建议用**陶瓷天线模块**（自带匹配），降低设计难度

#### 电源注意事项

- [ ] nRF52840 的 DEC1-DEC6 去耦电容**必须按数据手册放置**，靠近引脚
- [ ] 如果用内部 DC-DC 模式（推荐，省电），需要外部电感 (10μH) 在 DCC 引脚
- [ ] USB 供电的 VBUS 检测引脚要正确连接

### 开发建议

1. **先用开发板验证固件**，确认键盘矩阵、蓝牙、USB 都正常工作
2. **自制 PCB 第一版先做简单验证板**：nRF52840 最小系统 + SWD + USB + 几个 GPIO 测试点
3. **参考 Nordic 官方 nRF52840-DK (PCA10056) 原理图**，这是最权威的参考设计
4. **参考 Nice!Nano 开源原理图** (如果有) 或 SuperMini NRF52840 原理图
5. **J-Link 先买一个**，自制 PCB 必须通过 SWD 首次烧录，出问题也需要 SWD 救砖

## 已知问题与解决方案

### ✅ 同列按键组合失效 (已修复: 高阻态扫描)

**现象**: 无二极管 dome sheet 矩阵中，同列两键同时按下时检测不到（如 Shift+E, Fn+ESC）。

**根因**: ZMK 默认 kscan 驱动将不活跃 row 驱动 LOW，无二极管时同列按键会通过 dome 开关将 column 线短路到 LOW 的 row，导致读不出 HIGH。

**修复**: 修改 `kscan_gpio_matrix.c`，扫描时将不活跃 row 切换为 GPIO_INPUT（高阻态），与 QMK 行为一致。

### ✅ iOS 蓝牙扫描显示旧设备名

**现象**: 改了 BLE 广播名后，iOS 蓝牙设置扫描列表仍显示旧名字 "KeebDeck Basic"。

**根因**: iOS 根据 BLE MAC 地址缓存设备名。即使固件广播新名字，iOS 扫描界面不会更新已知 MAC 的显示名，直到建立一次连接。

**解决**: 直接点击配对/连接，连接成功后 iOS 自动刷新为新名字 "CyberFly"。

**注意**: 这不是固件问题，是 iOS 系统行为。nRF Connect app 可以看到真实广播名，不受缓存影响。

### ✅ 电池模式下禁用 PWM 背光

**策略**: 当检测到非 USB 供电（即 BLE + 电池模式）时，固件强制关闭 PWM 背光 LED，用户无法通过 BL_CYCLE 手动开启。

**原因**: AP3032 boost 驱动 + 键盘背光 LED 耗电极大，电池模式下开启背光会迅速耗尽电量。nRF52840 本身功耗约 5-10mA（BLE 广播态），而背光在 30% 亮度下额外消耗 30-50mA+，电池续航会从数周降至数小时。

**实现**: `app/src/backlight.c` 的 `zmk_backlight_update()` 中检查 `zmk_usb_is_powered()`，非 USB 供电时强制 brt=0。同时启用 `CONFIG_ZMK_BACKLIGHT_AUTO_OFF_USB=y`，使 USB 插拔事件自动触发背光状态刷新。

**行为**:
- 插 USB: 背光正常工作，可通过 Fn+Space 循环调节亮度
- 拔 USB (电池): 背光立即关闭，Fn+Space 仍可调整亮度值（NVS 保存），但 PWM 输出始终为 0
- 重新插 USB: 背光恢复为之前保存的亮度

### Bootloader 呼吸灯说明

进入 UF2 Bootloader 后看到背光做呼吸效果，这是 **nice_nano bootloader 自身的代码**，不是 ZMK 固件：

- 实现文件: `nice-bootloader/src/boards/boards.c` → `led_tick()`
- 驱动引脚: P0.15 (硬件 PWM0 三角波)
- 呼吸速度:
  - USB 未挂载: 300ms 周期 (快脉冲)
  - USB 已挂载: 3000ms 周期 (慢呼吸)
  - 写入 UF2 中: 100ms 周期 (快速闪烁)
- 最大亮度: 硬编码 31% (`0x4f/0xff`)
- 不可运行时配置，修改需重新编译 bootloader

## 注意事项

- Zephyr SDK 1.0.0 与当前 Zephyr 4.1.0 **不兼容**，必须使用 0.16.x
- `setup.sh` 需要交互输入，本地环境**不需要运行**它，只要 `ZEPHYR_SDK_INSTALL_DIR` 指向 SDK 目录即可
- macOS 首次识别键盘会弹出"键盘设置助理"，按提示操作一次后不会再弹
- 全新 nRF52840 芯片**必须通过 SWD 首次烧录 Bootloader**，之后才能用 UF2 U盘模式刷固件
