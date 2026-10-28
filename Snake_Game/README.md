# Snake Game — STM32F103 + FreeRTOS + SSD1306

**English** | [简体中文](README.zh-CN.md)

A small bare-metal game project: a **snake game on a 128×64 I2C OLED**, steered by a
**rotary encoder**, with a **top-10 high score table stored in internal flash**
(survives power cycles), plus a **temperature / humidity screen** sharing the same I2C bus.

Built with STM32CubeMX, CMake and FreeRTOS (CMSIS-RTOS v2).

---

## Features

- **Boot menu** — `Snake Game` / `Temp/Humidity`
- **Snake**
  - 32 × 16 play field (4 × 4 px per cell), full screen
  - starts with length 2, head in the middle, random initial direction and food
  - one move per second (`SNAKE_TICK_MS`)
  - rotary encoder turns the head by 90° — only the **net rotation** of one move
    cycle is used, so spinning fast can never flip the snake around
  - 180° reversal is impossible (turning onto the neck is rejected)
  - eating a food pellet grows the snake by one segment and adds one point
  - hitting a wall or itself ends the game
- **High scores** — top 10 kept in the last 1 KB flash page, checksummed and
  validated on boot; scrolling list with a selection cursor
- **Environment screen** — AHT20/AHT10 read every 2 s on the shared I2C1 bus
- **Key driver** — debounced, event based (short / long / optional double click) and
  delivered through FreeRTOS message queues
- **Bus arbitration** — a mutex serialises all I2C1 traffic between the display and
  the sensor task

---

## Hardware

| Part | Detail |
|---|---|
| MCU | STM32F103C8T6 — Cortex-M3, 72 MHz, 64 KB flash, 20 KB RAM |
| Display | SSD1306 128×64 OLED, I2C1 (PB6 = SCL, PB7 = SDA), 100 kHz, address `0x3C` |
| Sensor | AHT20 / AHT10, same I2C1 bus, address `0x38` |
| Keys | KEY1 = PB12, KEY2 = PB13, input with pull-up, active low |
| Encoder | TIM1 CH1/CH2 on PA8/PA9, encoder mode TI12, input filter 15 |
| Debug | SWD on PA13/PA14 |
| Debugger | ST-Link V2 (OpenOCD) |

---

## Controls

### Main menu

| Key | Action |
|---|---|
| KEY1 short | move selection down |
| KEY2 short | move selection up |
| KEY1 long | enter the selected screen |

### Snake game menu

| Key | Action |
|---|---|
| KEY1 short | next high score entry |
| KEY1 long | previous high score entry |
| KEY2 short | **start a new game** |

### In game

| Input | Action |
|---|---|
| Encoder clockwise | turn right 90° |
| Encoder counter-clockwise | turn left 90° |
| KEY1 long | abort and go back to the game menu |
| automatic | one move per second |

After a game ends the score is stored, `GAME OVER` is shown for 2 s and the
high score menu returns.

### Temperature / humidity screen

| Key | Action |
|---|---|
| any key | back to the main menu |

---

## Software architecture

### FreeRTOS objects

| Object | Type | Priority / size |
|---|---|---|
| `defaultTask` | task | Normal, 128 words |
| `myOLEDTasj` | task — UI state machine, draw, game loop | Low, 256 words |
| `myKEYTask` | task — key debounce and events | Low, 128 words |
| `myTempTask` | task — AHT2x sampling | Low, 128 words |
| `myEncoderTask` | task — TIM1 counter polling | Low, 128 words |
| `KEY1Queue`, `KEY2Queue` | message queue, 8 × `uint16_t` | — |
| `I2C1Mutex` | mutex, priority inheritance | — |

`configTOTAL_HEAP_SIZE` must be **8192 bytes** — five tasks, the timer service
task, two queues, one mutex and the idle task do not fit in the 3072 byte default.

### Design notes

- **Single owner of the panel.** Only the UI task touches the display; the frame
  buffer (`OLED_GRAM`) needs no locking. Only `OLED_ShowFrame()` — the part that
  drives I2C — is wrapped by the bus mutex.
- **Event driven keys.** The key task only reports events (`KEY_EVT_SHORT`,
  `KEY_EVT_LONG`, `KEY_EVT_DOUBLE`); nobody polls the GPIOs from the UI.
- **Encoder sampled once per move.** `ENCODER_TakeSteps()` returns the net number
  of detents since the previous move, so input volume never matters.
- **Pure game logic.** `Snake.c` has no hardware or RTOS dependency; it is a plain
  grid state machine that the UI task drives.

---

## Directory layout

```
Core/
├─ Inc/
│   ├─ EncoderTask.h      encoder task interface
│   ├─ I2CBus.h           I2C1 bus lock (Lock/Unlock)
│   ├─ KEYTask.h          key events and queue handles
│   ├─ Score.h            high score table
│   ├─ Snake.h            snake game logic
│   ├─ TempTask.h         sensor data interface
│   ├─ oled.h / font.h    display driver and fonts
│   └─ main.h, i2c.h, gpio.h, tim.h, FreeRTOSConfig.h   (CubeMX generated)
└─ Src/
    ├─ Game/
    │   ├─ Snake.c        grid, direction, food, collision
    │   └─ Score.c        top-10 table + flash persistence
    ├─ Task/
    │   ├─ I2C.c          mutex handle, I2C1Bus_Lock / I2C1Bus_Unlock
    │   ├─ EncoderTask.c  TIM1 encoder polling
    │   ├─ KEYTask.c      key state machine
    │   └─ I2C/
    │       ├─ OLEDTask.c UI state machine, all screens, game loop
    │       └─ TempTask.c AHT2x driver
    ├─ oled.c, font.c     display driver and font data
    └─ main.c, gpio.c, i2c.c, tim.c, freertos.c, ...      (CubeMX generated)
```

The project file is `Snake_Game.ioc` — open it in STM32CubeMX to change pinout,
clocks, RTOS objects or the heap size.

---

## Build

Requirements: `arm-none-eabi-gcc`, CMake ≥ 3.22, Ninja
(the STM32 VS Code extension bundles all three).

```bash
cmake --preset Debug
cmake --build --preset Debug
```

The image is produced at `build/Debug/Snake_Game.elf`.
`Release` works the same way.

From VS Code with the CMake Tools extension you can also use the status bar
**Build** button.

---

## Flash

The repository ships a VS Code task in `.vscode/tasks.json`:

- `Ctrl+Shift+B` → build, then flash (default build task)

Or from a terminal (ST-Link + OpenOCD):

```bash
openocd -f interface/stlink.cfg -c "transport select swd" \
        -f target/stm32f1x.cfg \
        -c "program {build/Debug/Snake_Game.elf} verify reset exit"
```

> The path passed to `-c` is parsed by Tcl, so it must be wrapped in braces
> (`{...}`) or use forward slashes — otherwise backslashes are eaten as escapes.

---

## Notes and gotchas

1. **Keep SYS → Debug = Serial Wire** in CubeMX. With `No_Debug` the generated
   `HAL_MspInit()` calls `__HAL_AFIO_REMAP_SWJ_DISABLE()`, the SWD pins are turned
   off right after reset and the board can only be recovered by booting with
   `BOOT0 = 1`.
2. **Task entry names must match.** CubeMX generates a `__weak` stub per task; the
   implementation only overrides it when the function name is identical
   (`StartOLEDTask`, `StartKEYTask`, `StartTempTask`, `StartEncoderTask`).
3. **`TOTAL_HEAP_SIZE = 8192`** must be set in *FREERTOS → Config parameters*,
   otherwise regenerating the code reverts `FreeRTOSConfig.h`.
4. **High scores live at `0x0800FC00`** (last 1 KB page). Keep the image below
   63 KB — currently about 37 KB.
5. **Encoder tuning** — `Core/Src/Task/EncoderTask.c`:
   - `ENCODER_COUNTS_PER_STEP` (default 4): one detent = 4 counts. Use 2 or 8 if a
     single click turns twice or several clicks are needed.
   - `ENCODER_INVERT` (default 1): flips clockwise / counter-clockwise.
6. **The font has no CJK glyphs.** `zh16x16` only contains a few characters, so the
   UI is English. Regenerate `Core/Src/font.c` with
   <https://led.baud-dance.com> (16×16, all characters at once) if you need Chinese.
7. **I2C runs at 100 kHz**, so a full frame takes ≈ 93 ms. Raising it to 400 kHz
   makes the UI noticeably snappier if the module has adequate pull-ups.
8. **Game speed** — `SNAKE_TICK_MS` in `OLEDTask.c` is 1000 ms; 400–500 ms feels
   much more like a real snake game.

---

## Credits

- Display driver and font handling: 波特律动 (Baud Dance) OLED library.
- HAL initialisation, FreeRTOS objects and the CMake skeleton are generated by
  STM32CubeMX (STM32Cube FW_F1 V1.8.7).