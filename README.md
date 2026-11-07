# ESP-12E Breadboard MP3 Player

A standalone MP3 player built with a NodeMCU ESP-12E, microSD-card reader, MAX98357A I²S amplifier, OLED display, speaker, and three push-buttons.

The player reads MP3 files from a microSD card, displays playback information on the OLED, and supports play/pause, next, and previous-track controls.

## Features

- Plays root-folder MP3 files from a FAT32 microSD card
- Automatically advances to the next track
- 0.96-inch I²C OLED playback display
- Play/pause, next, and previous controls
- MAX98357A I²S digital amplifier output
- Fixed 100% software volume
- Supports up to 32 MP3 files

## Hardware required

- NodeMCU ESP-12E / ESP8266 development board
- MAX98357A I²S Class-D amplifier module
- 4–8 Ω speaker, rated up to 3 W
- SPI microSD-card reader module and FAT32-formatted microSD card
- 0.96-inch SSD1306 I²C OLED display (128 × 64, typically `0x3C`)
- 3 × momentary push-buttons, breadboard, jumper wires, and a stable 5 V USB supply

## Wiring

### OLED display

| OLED pin | NodeMCU pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | D1 / GPIO5 |
| SDA | D2 / GPIO4 |

### MicroSD reader

| SD reader pin | NodeMCU pin |
|---|---|
| CS | D0 / GPIO16 |
| SCK / CLK | D5 / GPIO14 |
| MISO / DO | D6 / GPIO12 |
| MOSI / DI | D7 / GPIO13 |
| GND | GND |
| VCC | 5V for a 5V-capable module; otherwise 3V3 |

### MAX98357A amplifier

| MAX98357A pin | NodeMCU pin |
|---|---|
| VIN | 5V |
| GND | GND |
| LRC | D4 / GPIO2 |
| BCLK | D8 / GPIO15 |
| DIN | RX / GPIO3 |
| SPK+ | Speaker positive terminal |
| SPK− | Speaker negative terminal |

> Connect the speaker only between `SPK+` and `SPK−`; never connect either speaker lead to GND.

For generic MAX98357A modules, connect `SD` to `3V3` if there is no sound. A floating `SD` pin can leave some modules in shutdown. Leave `GAIN` unconnected unless you want to change the amplifier's hardware gain.

### Buttons

| Function | Wiring |
|---|---|
| Play/pause | D3 / GPIO0 to GND |
| Next | TX / GPIO1 to GND |
| Previous | Connects D3 / GPIO0 to TX / GPIO1 |

## Software setup

1. Install **ESP8266 by ESP8266 Community** through Arduino IDE's **Boards Manager**.
2. Select **NodeMCU 1.0 (ESP-12E Module)** from **Tools → Board**.
3. Install these libraries through **Library Manager**:
   - `ESP8266Audio` by Earle F. Philhower, III
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`
4. Open [MP3_Player_ESP12E.ino](MP3_Player_ESP12E.ino).
5. Use these board settings:
   - CPU Frequency: **160 MHz**
   - Upload Speed: **115200**
   - Flash Size: your board's normal 4 MB setting
6. Select the NodeMCU serial port and click **Upload**.

## Preparing the microSD card

1. Format the card as **FAT32**.
2. Copy MP3 files directly to its root folder—do not put them inside a subfolder.
3. Prefix files to choose their play order:

```text
01_Intro.mp3
02_Favourite_Song.mp3
03_Outro.mp3
```

## Controls

| Button | Action |
|---|---|
| Play/pause | Starts, pauses, or resumes the current track |
| Next | Starts the next MP3 file |
| Previous | Starts the preceding MP3 file |

## Troubleshooting

### Screen shows playback but there is no audio

- Connect the MAX98357A `SD` pin to `3V3`, then power-cycle the board.
- Confirm `LRC → D4`, `BCLK → D8`, and `DIN → RX`.
- Confirm the speaker is connected across `SPK+` and `SPK−`, not to GND.
- Confirm the amplifier has 5V and a shared ground with the NodeMCU.

### SD card is not detected

- Check the `CS`, `SCK`, `MISO`, and `MOSI` connections against the table above.
- Ensure the card is FAT32-formatted and inserted correctly.
- Use 3V3 power if the SD module is labelled 3.3V-only.

### Board will not boot or upload

Do not hold Play/pause while powering, resetting, or uploading. It uses GPIO0, an ESP8266 boot-mode pin.

## Project structure

```text
.
├── mp3-player/
│   └── MP3_Player_ESP12E.ino
└── README.md
```

## License

Provided for educational and personal-use projects. Feel free to adapt it for your own builds.
