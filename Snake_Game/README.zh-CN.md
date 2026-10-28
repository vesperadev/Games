# 贪吃蛇 — STM32F103 + FreeRTOS + SSD1306

**简体中文** | [English](README.md)

一个跑在 STM32F103C8T6 上的小游戏项目：**128×64 I2C OLED 上的贪吃蛇**，
用**旋转编码器**控制蛇头转向，**最高分前 10 名保存在片内 Flash**（掉电不丢），
另外还有一个**温湿度显示界面**，与屏幕共用同一条 I2C 总线。

基于 STM32CubeMX + CMake + FreeRTOS（CMSIS-RTOS v2）构建。

---

## 功能

- **启动菜单** —— `Snake Game` / `Temp/Humidity`
- **贪吃蛇**
  - 32 × 16 棋盘（每格 4 × 4 像素），占满整屏
  - 初始长度 2，蛇头从屏幕中心出发，初始方向和食物都随机
  - 每秒移动一格（`SNAKE_TICK_MS`）
  - 旋转编码器让蛇头转 90°——只取两次移动之间的**净转动量**，
    所以拧多快都不会甩过头
  - 不可能 180° 掉头（踩到脖子直接判死）
  - 吃到食物长度 +1、得分 +1
  - 撞墙或撞到自己游戏结束
- **最高分** —— 前 10 名存在 Flash 最后一页，带 magic 和校验和，开机自动校验；
  列表带选择光标，可上下翻看
- **温湿度界面** —— AHT20/AHT10，每 2 秒采样一次，与屏幕共用 I2C1
- **按键驱动** —— 消抖 + 事件化（短按 / 长按 / 可选双击），通过 FreeRTOS 消息队列投递
- **总线仲裁** —— 用互斥量把屏幕和传感器的 I2C1 访问串行化

---

## 硬件

| 部件 | 说明 |
|---|---|
| MCU | STM32F103C8T6 —— Cortex-M3，72 MHz，64 KB Flash，20 KB RAM |
| 屏幕 | SSD1306 128×64 OLED，I2C1（PB6 = SCL，PB7 = SDA），100 kHz，地址 `0x3C` |
| 传感器 | AHT20 / AHT10，同一条 I2C1 总线，地址 `0x38` |
| 按键 | KEY1 = PB12，KEY2 = PB13，上拉输入，低电平有效 |
| 编码器 | TIM1 CH1/CH2（PA8/PA9），编码器模式 TI12，输入滤波 15 |
| 调试口 | SWD：PA13/PA14 |
| 调试器 | ST-Link V2（OpenOCD） |

---

## 操作说明

### 主菜单

| 按键 | 动作 |
|---|---|
| KEY1 短按 | 选择项下移 |
| KEY2 短按 | 选择项上移 |
| KEY1 长按 | 进入所选界面 |

### 贪吃蛇菜单

| 按键 | 动作 |
|---|---|
| KEY1 短按 | 下一条最高分记录 |
| KEY1 长按 | 上一条最高分记录 |
| KEY2 短按 | **开始游戏** |

### 游戏中

| 输入 | 动作 |
|---|---|
| 编码器顺时针 | 右转 90° |
| 编码器逆时针 | 左转 90° |
| KEY1 长按 | 中止游戏，返回游戏菜单 |
| 自动 | 每秒前进一格 |

游戏结束后记录本局得分，屏幕显示 `GAME OVER` 2 秒，然后回到最高分列表。

### 温湿度界面

| 按键 | 动作 |
|---|---|
| 任意键 | 返回主菜单 |

---

## 软件结构

### FreeRTOS 对象

| 对象 | 类型 | 优先级 / 栈 |
|---|---|---|
| `defaultTask` | 任务 | Normal，128 字 |
| `myOLEDTasj` | 任务 —— 界面状态机、绘制、游戏主循环 | Low，256 字 |
| `myKEYTask` | 任务 —— 按键消抖与事件生成 | Low，128 字 |
| `myTempTask` | 任务 —— AHT2x 采样 | Low，128 字 |
| `myEncoderTask` | 任务 —— TIM1 计数轮询 | Low，128 字 |
| `KEY1Queue`、`KEY2Queue` | 消息队列，8 × `uint16_t` | — |
| `I2C1Mutex` | 互斥量，带优先级继承 | — |

`configTOTAL_HEAP_SIZE` 必须是 **8192 字节** —— 5 个任务 + 定时器服务任务 +
2 个队列 + 1 个互斥量 + 空闲任务，默认的 3072 装不下。

### 设计要点

- **屏幕单一所有者**：只有 UI 任务操作屏幕，显存（`OLED_GRAM`）不需要加锁；
  只有真正走 I2C 的 `OLED_ShowFrame()` 才被总线互斥量包住。
- **按键事件化**：按键任务只负责产生 `KEY_EVT_SHORT` / `KEY_EVT_LONG` /
  `KEY_EVT_DOUBLE` 事件，UI 侧不轮询 GPIO。
- **编码器每次移动只采样一次**：`ENCODER_TakeSteps()` 返回距上次移动的净格数，
  所以输入多少都不影响结果。
- **游戏逻辑纯函数化**：`Snake.c` 不依赖任何硬件和 RTOS，就是一个网格状态机，
  由 UI 任务驱动。

---

## 目录结构

```
Core/
├─ Inc/
│   ├─ EncoderTask.h      编码器任务接口
│   ├─ I2CBus.h           I2C1 总线锁（Lock / Unlock）
│   ├─ KEYTask.h          按键事件与队列句柄
│   ├─ Score.h            最高分表
│   ├─ Snake.h            贪吃蛇逻辑
│   ├─ TempTask.h         传感器数据接口
│   ├─ oled.h / font.h    OLED 驱动与字库
│   └─ main.h, i2c.h, gpio.h, tim.h, FreeRTOSConfig.h   （CubeMX 生成）
└─ Src/
    ├─ Game/
    │   ├─ Snake.c        网格、方向、食物、碰撞
    │   └─ Score.c        前十名 + Flash 持久化
    ├─ Task/
    │   ├─ I2C.c          互斥量句柄、I2C1Bus_Lock / I2C1Bus_Unlock
    │   ├─ EncoderTask.c  TIM1 编码器轮询
    │   ├─ KEYTask.c      按键状态机
    │   └─ I2C/
    │       ├─ OLEDTask.c 界面状态机、各页面、游戏主循环
    │       └─ TempTask.c AHT2x 驱动
    ├─ oled.c, font.c     OLED 驱动与点阵数据
    └─ main.c, gpio.c, i2c.c, tim.c, freertos.c, ...      （CubeMX 生成）
```

工程文件是 `Snake_Game.ioc` —— 引脚、时钟、RTOS 对象、堆大小都在 STM32CubeMX 里改。

---

## 构建

需要：`arm-none-eabi-gcc`、CMake ≥ 3.22、Ninja
（安装 STM32 的 VS Code 扩展会自带这三样）。

```bash
cmake --preset Debug
cmake --build --preset Debug
```

产物在 `build/Debug/Snake_Game.elf`，`Release` 用法相同。

在装了 CMake Tools 扩展的 VS Code 里，也可以直接点状态栏的 **Build**。

---

## 烧录

仓库自带 `.vscode/tasks.json`：

- `Ctrl+Shift+B` → 先编译再烧录（默认构建任务）

或者在终端用 ST-Link + OpenOCD：

```bash
openocd -f interface/stlink.cfg -c "transport select swd" \
        -f target/stm32f1x.cfg \
        -c "program {build/Debug/Snake_Game.elf} verify reset exit"
```

> `-c` 后面的字符串由 Tcl 解析，路径必须用花括号 `{...}` 包住（或者用正斜杠），
> 否则反斜杠会被当成转义符吃掉。

---

## 注意事项（踩过的坑）

1. **CubeMX 里 SYS → Debug 必须保持 Serial Wire**。如果设成 `No_Debug`，
   生成的 `HAL_MspInit()` 会调用 `__HAL_AFIO_REMAP_SWJ_DISABLE()`，
   芯片一上电就关掉 SWD，之后只能靠 `BOOT0 = 1` 进内置 bootloader 才能救回来。
2. **任务入口函数名必须一致**。CubeMX 为每个任务生成一个 `__weak` 空实现，
   只有同名强函数才能覆盖它（`StartOLEDTask`、`StartKEYTask`、
   `StartTempTask`、`StartEncoderTask`）。
3. **`TOTAL_HEAP_SIZE = 8192`** 要在 *FREERTOS → Config parameters* 里设置，
   否则重新生成代码会把 `FreeRTOSConfig.h` 改回默认值。
4. **最高分存在 `0x0800FC00`**（最后一页 1 KB）。固件要保持在 63 KB 以内，
   目前约 37 KB。
5. **编码器调参**都在 `Core/Src/Task/EncoderTask.c`：
   - `ENCODER_COUNTS_PER_STEP`（默认 4）：一格 = 4 个计数。
     如果拧一格转两次，改成 8；拧好几格才转一次，改成 2。
   - `ENCODER_INVERT`（默认 1）：翻转顺/逆时针。
6. **字库里没有汉字**。`zh16x16` 只包含几个字，所以界面用英文。
   需要中文的话，用 <https://led.baud-dance.com> 重新生成 `Core/Src/font.c`
   （16×16，所有字一次性生成）。
7. **I2C 目前跑 100 kHz**，整屏刷新约 93 ms。若模块上拉足够，提到 400 kHz
   界面会明显跟手。
8. **游戏速度**：`OLEDTask.c` 里的 `SNAKE_TICK_MS` 是 1000 ms，
   改成 400~500 ms 更接近正常贪吃蛇的手感。

---

## 致谢

- OLED 驱动与字库处理：波特律动（Baud Dance）OLED 库。
- HAL 初始化、FreeRTOS 对象与 CMake 骨架由 STM32CubeMX 生成
  （STM32Cube FW_F1 V1.8.7）。