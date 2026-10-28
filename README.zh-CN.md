# Games

面向嵌入式硬件的小游戏合集——每个游戏独立成目录、自带完整文档，可直接在低成本开发板上运行。

## 游戏列表

| 游戏 | 平台 | 亮点 |
|------|------|------|
| [Snake_Game](./Snake_Game/) | STM32F103 + SSD1306 OLED | 经典贪吃蛇 · EC11 旋钮编码器转向 · FreeRTOS（CMSIS-RTOS v2）· 前十名高分持久化到内部 Flash · AHT20 温湿度同屏显示 |

## 目标硬件

所有游戏均面向 **KEYSKING STM32 入门开发板**：

- **MCU：** STM32F103C8T6 — ARM Cortex-M3 @ 72 MHz，64 KB Flash，20 KB SRAM
- **显示屏：** 128×64 I2C OLED（SSD1306）
- **输入：** EC11 旋钮编码器、独立按键

每个游戏的 README 均包含完整的引脚分配与片上外设使用说明。

## 构建要求

每个游戏在其子目录内自包含：

- `arm-none-eabi-gcc`
- `CMake >= 3.22`
- `Ninja`

```bash
cd Snake_Game
cmake --preset Debug
cmake --build --preset Debug
```

## 烧录与调试

使用 ST-Link V2 + OpenOCD：

```bash
openocd -f interface/stlink.cfg -c "transport select swd" \
        -f target/stm32f1x.cfg \
        -c "program {build/Debug/Snake_Game.elf} verify reset exit"
```

## 目录结构

```
Games/
├── Snake_Game/      # 经典贪吃蛇（STM32F103 + FreeRTOS + SSD1306）
│   ├── Core/
│   ├── Drivers/
│   └── README.md
└── <Future_Game>/   # 更多游戏待加入
```

## 许可证

MIT —— 个别游戏目录如有额外条款，以该目录为准。
