# Snake Game — STM32F103 + FreeRTOS + SSD1306

**English** | [简体中文](README.zh-CN.md)

A bare-metal snake game for the **KEYSKING STM32 Tutorial** development board:
**128×64 I2C OLED** display, **EC11 rotary encoder** steering, **top-10 high scores**
persisted in internal flash (survives power cycles), plus a **temperature / humidity**
screen sharing the same I2C bus.

Built with STM32CubeMX, CMake and FreeRTOS (CMSIS-RTOS v2).

---

## What this project does

- **Boot menu** — `Snake Game` / `Temp/Humidity`
- **Snake game**
  - 32 × 16 play field (4 × 4 px per cell), full screen
  - starts with length 2, head centred, random initial direction and food
  - one move per second (`SNAKE_TICK_MS` in `OLEDTask.c`)
  - EC11 encoder turns the head 90° per detent
  - only the **net rotation** between two moves is consumed, so spinning fast can never flip the snake
  - 180° reversal is impossible (attempting to step onto the neck is rejected)
  - eating a food pellet grows the snake by one segment, +1 point
  - hitting a wall or itself ends the game
- **High scores** — top 10 kept in the last 1 KB flash page, magic- and checksum-validated on boot; scrolling list with selection cursor
- **Environment screen** — AHT20/AHT10 read every 2 s on the shared I2C1 bus
- **Key driver** — debounced, event-based (short / long) delivered through FreeRTOS message queues
- **Bus arbitration** — mutex serialises all I2C1 traffic between OLED and sensor
- **Death-safe flash** — `HAL_FLASH_Lock()` is guaranteed even if erase or program fails mid-way (`goto cleanup`)

---

## Hardware

### Development board

**KEYSKING STM32 Tutorial** — STM32F103C8T6 on LQFP48.

![Snake game boot menu on the board's OLED](images/board-oled-snake-menu.jpg)
*The snake-game boot menu running on the on-board 128×64 I2C OLED.*

![KEYSKING STM32 Tutorial board overview](images/board-overview.jpg)
*Board overview — this game only touches the OLED, the EC11 encoder and KEY1/KEY2.*

### MCU

| Item | Value |
|---|---|
| Core | ARM Cortex-M3, 72 MHz |
| Flash | 64 KB (last 1 KB page reserved for high scores) |
| RAM | 20 KB |
| Crystal | 8 MHz HSE, PLL × 9 → 72 MHz SYSCLK |
| Debug | SWD (PA13 / PA14) |

### Peripherals used

| Module | Interface | Pin / address | Notes |
|---|---|---|---|
| SSD1306 OLED | I2C1 | PB6 = SCL, PB7 = SDA, `0x3C` | 128×64, 100 kHz |
| AHT20 / AHT10 | I2C1 (shared) | PB6 / PB7, `0x38` | 2 s sampling period |
| EC11 encoder | TIM1 encoder mode TI12 | PA8 = CH1, PA9 = CH2 | 4 counts per detent, filter = 15 |
| KEY1 | GPIO input, pull-up, active-low | PB12 | short / long press |
| KEY2 | GPIO input, pull-up, active-low | PB13 | short / long press |

### Full pin allocation

```
PA8  ── TIM1_CH1   (EC11 encoder A)
PA9  ── TIM1_CH2   (EC11 encoder B)
PA13 ── SWDIO      (ST-Link debug)
PA14 ── SWCLK      (ST-Link debug)
PB6  ── I2C1_SCL   (OLED SCL + AHT20 SCL)
PB7  ── I2C1_SDA   (OLED SDA + AHT20 SDA)
PB12 ── KEY1       (pull-up, active-low)
PB13 ── KEY2       (pull-up, active-low)
PD0  ── HSE_IN     (8 MHz crystal)
PD1  ── HSE_OUT    (8 MHz crystal)
```

### On-board but **not used** by this project

The KEYSKING board ships many more peripherals — this game only exercises five of them:

| Module | Description |
|---|---|
| NTC | Thermistor, voltage divider |
| BEEP | Active buzzer |
| VOL | Potentiometer |
| RGB | WS2812 or similar addressable LED |
| RELAY | SRD-05VDC-SL-C 5 V relay |
| SERVO | RC servo header |
| TCRT5000 | Infrared line-following sensor |
| BLUE | Bluetooth module header (HC-05 / CH340) |
| DRV8833 | Dual H-bridge motor driver |
| HC-SR04 | Ultrasonic distance sensor |

Everything above can be wired up without touching the snake-game code — the board's GPIO and power rails are already broken out.

---

## Controls

### Main menu

| Input | Action |
|---|---|
| KEY1 short | move selection down |
| KEY2 short | move selection up |
| KEY1 long | enter the selected screen |

### Snake game menu

| Input | Action |
|---|---|
| KEY1 short | next high-score entry |
| KEY1 long | previous high-score entry |
| KEY2 short | **start a new game** |

### In game

| Input | Action |
|---|---|
| EC11 clockwise | turn right 90° |
| EC11 counter-clockwise | turn left 90° |
| KEY1 long | abort, return to game menu |
| (automatic) | one move per second |

After death the score is stored, `GAME OVER` is shown for 2 s, then the high-score menu returns.

### Temperature / humidity screen

| Input | Action |
|---|---|
| any key | back to main menu |

---

## Software architecture

### FreeRTOS objects

| Object | Type | Stack / size | Notes |
|---|---|---|---|
| `defaultTask` | task | 128 words | CubeMX auto-generated shell, not used |
| `myOLEDTasj` | task — UI state machine, draw, game loop | **256 words** | highest consumer (HAL I2C inside `OLED_ShowFrame`) |
| `myKEYTask` | task — key debounce & events | 128 words | |
| `myTempTask` | task — AHT2x sampling | 128 words | HAL I2C calls |
| `myEncoderTask` | task — TIM1 counter polling | 128 words | |
| `KEY1Queue`, `KEY2Queue` | message queue, 8 × `uint16_t` | — | dropped when full (non-blocking put) |
| `I2C1Mutex` | mutex, priority inheritance | — | serialises OLED ↔ AHT20 on I2C1 |

`configTOTAL_HEAP_SIZE = 8192` bytes — five tasks + timer service task + two queues + one mutex + idle task. **Do not reduce** without running `uxTaskGetStackHighWaterMark` first.

### Concurrency model

| Shared data | Writer | Reader | Protection |
|---|---|---|---|
| `s_counts` (encoder) | EncoderTask | OLEDTask | `volatile` + `__disable_irq` + read-and-clear (no read-modify-write) |
| `s_data` / `s_seq` (temp) | TempTask | OLEDTask | `volatile` + snapshot to local inside critical section, handed out after re-enable |
| Snake state (`s_body`, `s_dir`, ...) | OLEDTask (game loop) | OLEDTask (draw) | single task, no race |
| Score RAM (`s_blob`) | OLEDTask (on death) | OLEDTask (menu) | single task, no race |
| I2C1 bus | OLEDTask + TempTask | — | `I2C1Mutex` (FreeRTOS) |
| Flash registers | `blob_save` | — | single task + `goto cleanup` guarantees `HAL_FLASH_Lock` on every exit path |

### Design principles

- **Single owner of the panel.** Only the UI task touches the frame buffer (`OLED_GRAM`); drawing needs no locking. Only `OLED_ShowFrame()` — the part that actually drives I2C — is wrapped by the bus mutex.
- **Event-driven keys.** The key task only produces events (`KEY_EVT_SHORT`, `KEY_EVT_LONG`); nobody polls GPIOs from the UI.
- **Encoder sampled once per move.** `ENCODER_TakeSteps()` returns the net number of detents since the last call, so input volume never matters.
- **Pure game logic.** `Snake.c` has zero hardware or RTOS dependency — it is a plain grid state machine that the UI task drives.

---

## Directory layout

```
Snake_Game/
├─ Core/
│  ├─ Inc/
│  │  ├─ EncoderTask.h      encoder task interface
│  │  ├─ I2CBus.h           I2C1Bus_Lock() / I2C1Bus_Unlock()
│  │  ├─ KEYTask.h          key events and queue handles
│  │  ├─ Score.h            high score table (Top 10)
│  │  ├─ Snake.h            snake game logic
│  │  ├─ TempTask.h         sensor data interface
│  │  ├─ oled.h / font.h    SSD1306 driver + fonts (Baud Dance)
│  │  └─ CubeMX generated   main.h, i2c.h, gpio.h, tim.h, FreeRTOSConfig.h
│  └─ Src/
│     ├─ Game/
│     │  ├─ Snake.c         grid, direction, food, collision (no HAL / no OS)
│     │  └─ Score.c         Top 10 table + Flash persistence (goto cleanup safe)
│     ├─ Task/
│     │  ├─ I2C.c           I2C1Bus_Lock / I2C1Bus_Unlock (wraps I2C1Mutex)
│     │  ├─ EncoderTask.c   TIM1 encoder polling + atomic s_counts
│     │  ├─ KEYTask.c       key state machine, debounce, event dispatch
│     │  └─ I2C/
│     │     ├─ OLEDTask.c   UI state machine, all screens, game loop
│     │     └─ TempTask.c   AHT2x driver, snapshot-safe s_data/s_seq
│     ├─ oled.c / font.c    SSD1306 driver + 8×6 / 16×8 fonts
│     └─ CubeMX generated   main.c, gpio.c, i2c.c, tim.c, freertos.c, MSP files
├─ Drivers/                  STM32Cube HAL + CMSIS
├─ Middlewares/              FreeRTOS (CMSIS-RTOS v2)
├─ CMakeLists.txt            (and CMakePresets.json)
├─ Snake_Game.ioc            ← open in STM32CubeMX to reconfigure
├─ STM32F103XX_FLASH.ld      linker script
└─ startup_stm32f103xb.s
```

---

## Build

Requirements: `arm-none-eabi-gcc`, CMake ≥ 3.22, Ninja (the STM32 VS Code extension bundles all three).

```bash
cmake --preset Debug
cmake --build --preset Debug
```

Produces `build/Debug/Snake_Game.elf`. `Release` works identically.

From VS Code with the CMake Tools extension: use the **Build** button in the status bar.

---

## Flash & debug

### One-shot task (VS Code)

The repo ships a default build task in `.vscode/tasks.json`:

- `Ctrl+Shift+B` → build, then flash via OpenOCD

### Terminal (ST-Link V2 + OpenOCD)

```bash
openocd -f interface/stlink.cfg -c "transport select swd" \
        -f target/stm32f1x.cfg \
        -c "program {build/Debug/Snake_Game.elf} verify reset exit"
```

> The `-c` string is parsed by Tcl — wrap paths in braces `{...}` (or use forward slashes), otherwise backslashes are consumed as escapes.

### Flash usage

```
Section   Size
.text     ~31 KB   ← code + const data
.data      tiny
.bss       ~2 KB    ← OLED_GRAM + RTOS idle task + globals
───────────────────
Total     ~33 KB   flash used, 20 KB RAM used
```

---

## Tuning & notes

1. **CubeMX → SYS → Debug must stay Serial Wire.** Setting `No_Debug` generates `__HAL_AFIO_REMAP_SWJ_DISABLE()` — SWD pins are turned off at reset, the board can only be recovered by booting with `BOOT0 = 1`.
2. **Task entry names must match exactly.** CubeMX generates a `__weak` stub per task; your implementation overrides it only when the function name is identical (`StartOLEDTask`, `StartKEYTask`, `StartTempTask`, `StartEncoderTask`).
3. **`configTOTAL_HEAP_SIZE = 8192`** is set in *FREERTOS → Config parameters*. Re-generating code will revert `FreeRTOSConfig.h` unless this value is kept in CubeMX.
4. **High scores live at `0x0800FC00`** (the last 1 KB page of 64 KB flash). Keep the firmware **below 63 KB** — currently ~33 KB, comfortably safe.
5. **Encoder tuning** (`Core/Src/Task/EncoderTask.c`):
   - `ENCODER_COUNTS_PER_STEP` (default **4**): one mechanical detent = 4 TIM1 counts. Use 2 if one click turns twice, 8 if several clicks are needed.
   - `ENCODER_INVERT` (default **1**): flip clockwise ↔ counter-clockwise.
   - `ENCODER_SCAN_MS` (default **100**): polling period. 10 ms gives better responsiveness; 200 ms wastes less CPU.
6. **Concurrent variables must be `volatile`.** `s_counts`, `s_data`, `s_seq` are shared between tasks and read/written inside `__disable_irq` critical sections. Without `volatile` the compiler may cache them in registers and optimise away reads.
7. **Flash unlock → lock is non-negotiable.** `blob_save()` uses `goto cleanup` to guarantee `HAL_FLASH_Lock()` runs even when `HAL_FLASHEx_Erase` or `HAL_FLASH_Program` returns `HAL_ERROR`. A missed lock leaves the flash writable by accident — a stray I2C glitch could corrupt code.
8. **Game speed** — `SNAKE_TICK_MS` in `OLEDTask.c` is **1000 ms** (one move per second). 400–500 ms feels much closer to a classic snake game.
9. **I2C speed** — currently 100 kHz, full frame ≈ 93 ms. If your OLED module has pull-ups ≥ 4.7 kΩ, bump I2C1 to 400 kHz in CubeMX for noticeably snappier updates.
10. **No CJK in the font.** `zh16x16` only contains a handful of characters, so the UI is English. Regenerate `Core/Src/font.c` with <https://led.baud-dance.com> (16×16, all at once) if you need Chinese glyphs.

---

## Credits

- **Baud Dance OLED library** — display driver (`oled.c`) and font pipeline (`font.c`).
- **STM32CubeMX** — HAL initialisation, FreeRTOS objects, CMake skeleton.
- **KEYSKING** — the STM32 Tutorial board itself.

---

## License

MIT — see the repo's top-level `LICENSE` file.