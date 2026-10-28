# Games

A collection of small games built for embedded hardware — each game is self-contained in its own directory, fully documented, and ready to run on a low-cost development board.

## Game List

| Game | Platform | Highlights |
|------|----------|------------|
| [Snake_Game](./Snake_Game/) | STM32F103 + SSD1306 OLED | Classic snake game · EC11 rotary-encoder steering · FreeRTOS (CMSIS-RTOS v2) · top-10 high scores persisted in internal flash · AHT20 temperature / humidity screen |

## Target Hardware

All games target the **KEYSKING STM32 Tutorial** development board:

- **MCU:** STM32F103C8T6 — ARM Cortex-M3 @ 72 MHz, 64 KB flash, 20 KB SRAM
- **Display:** 128×64 I2C OLED (SSD1306)
- **Inputs:** EC11 rotary encoder, momentary push buttons

Each game's own README documents the full pin-out and peripheral usage.

## Build Requirements

Every game is self-contained in its own subdirectory:

- `arm-none-eabi-gcc`
- `CMake >= 3.22`
- `Ninja`

```bash
cd Snake_Game
cmake --preset Debug
cmake --build --preset Debug
```

## Flash & Debug

Using ST-Link V2 with OpenOCD:

```bash
openocd -f interface/stlink.cfg -c "transport select swd" \
        -f target/stm32f1x.cfg \
        -c "program {build/Debug/Snake_Game.elf} verify reset exit"
```

## Directory Layout

```
Games/
├── Snake_Game/      # Classic snake game (STM32F103 + FreeRTOS + SSD1306)
│   ├── Core/
│   ├── Drivers/
│   └── README.md
└── <Future_Game>/   # More games coming
```

## License

MIT — see each game's directory for any additional terms.
