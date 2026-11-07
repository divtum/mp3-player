/*
  ESP-12E / NodeMCU SD-card MP3 player

  Matches the supplied schematic:
    OLED: SDA=D2/GPIO4, SCL=D1/GPIO5, address 0x3C
    SD:   CS=D0/GPIO16, SCK=D5/GPIO14, MISO=D6/GPIO12, MOSI=D7/GPIO13
    I2S:  LRC=D4/GPIO2, BCLK=D8/GPIO15, DIN=RX/GPIO3
    Pot:  A0

  Button wiring in the supplied drawing is a two-pin matrix:
    Play/pause: D3 to GND
    Next:       TX to GND
    Previous:   D3 shorted to TX
  The scanButtonMatrix() routine decodes that matrix without an extra resistor.

  Put MP3 files in the ROOT of a FAT16/FAT32 microSD card.  Names beginning
  with 01_, 02_, ... are a simple way to control the play order.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// Pin numbers below are ESP8266 GPIO numbers, not the D-labels.
static const uint8_t PIN_OLED_SDA = 4;   // D2
static const uint8_t PIN_OLED_SCL = 5;   // D1
static const uint8_t PIN_SD_CS    = 16;  // D0
static const uint8_t PIN_BTN_D3   = 0;   // D3: play/pause or previous matrix leg
static const uint8_t PIN_BTN_TX   = 1;   // TX: next or previous matrix leg

static const uint8_t OLED_ADDRESS = 0x3C;
static const uint8_t SCREEN_WIDTH = 128;
static const uint8_t SCREEN_HEIGHT = 64;
static const uint8_t MAX_TRACKS = 32;
static const uint8_t TRACK_NAME_BYTES = 64;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
AudioFileSourceSD source;
AudioGeneratorMP3 mp3;
AudioOutputI2S i2s;

char tracks[MAX_TRACKS][TRACK_NAME_BYTES];
uint8_t trackCount = 0;
uint8_t trackIndex = 0;
bool paused = false;
bool sdReady = false;
const char *fatalMessage = nullptr;

enum Button : uint8_t { BTN_NONE, BTN_PLAY_PAUSE, BTN_NEXT, BTN_PREVIOUS };

void drawScreen();
void startTrack(uint8_t index);

bool isMP3(const char *name) {
  String lower(name);
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

void sortTracks() {
  for (uint8_t i = 0; i + 1 < trackCount; ++i) {
    for (uint8_t j = i + 1; j < trackCount; ++j) {
      if (strcmp(tracks[j], tracks[i]) < 0) {
        char temporary[TRACK_NAME_BYTES];
        memcpy(temporary, tracks[i], TRACK_NAME_BYTES);
        memcpy(tracks[i], tracks[j], TRACK_NAME_BYTES);
        memcpy(tracks[j], temporary, TRACK_NAME_BYTES);
      }
    }
  }
}

void findTracks() {
  trackCount = 0;
  File root = SD.open("/");
  if (!root) return;

  while (trackCount < MAX_TRACKS) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory() && isMP3(entry.name())) {
      strncpy(tracks[trackCount], entry.name(), TRACK_NAME_BYTES - 1);
      tracks[trackCount][TRACK_NAME_BYTES - 1] = '\0';
      ++trackCount;
    }
    entry.close();
  }
  root.close();
  sortTracks();
}

void stopTrack() {
  if (mp3.isRunning()) mp3.stop();
  if (source.isOpen()) source.close();
  paused = false;
}

void startTrack(uint8_t index) {
  if (!sdReady || trackCount == 0) return;

  stopTrack();
  trackIndex = index % trackCount;
  if (!source.open(tracks[trackIndex])) {
    fatalMessage = "Cannot open MP3";
    drawScreen();
    return;
  }

  // ESP8266 I2S is fixed in hardware to BCLK=GPIO15, LRC=GPIO2, DATA=GPIO3.
  // SetPinout documents the intended MAX98357A wiring and is harmless here.
  i2s.SetPinout(15, 2, 3);
  if (!mp3.begin(&source, &i2s)) {
    source.close();
    fatalMessage = "MP3 decoder error";
  } else {
    fatalMessage = nullptr;
  }
  drawScreen();
}

void nextTrack() {
  if (trackCount) startTrack((trackIndex + 1) % trackCount);
}

void previousTrack() {
  if (!trackCount) return;
  startTrack(trackIndex == 0 ? trackCount - 1 : trackIndex - 1);
}

/*
  A normal INPUT_PULLUP read cannot see the Previous button because it only
  connects the two pins.  Two safe, one-way probe reads produce this table:
      D3 low / TX pullup      D3 pullup / TX low
  None           HIGH                    HIGH
  Play           HIGH                    LOW
  Next           LOW                     HIGH
  Previous       LOW                     LOW
*/
Button scanButtonMatrix() {
  pinMode(PIN_BTN_D3, OUTPUT);
  digitalWrite(PIN_BTN_D3, LOW);
  pinMode(PIN_BTN_TX, INPUT_PULLUP);
  delayMicroseconds(3);
  bool txLow = digitalRead(PIN_BTN_TX) == LOW;

  pinMode(PIN_BTN_D3, INPUT_PULLUP);
  pinMode(PIN_BTN_TX, OUTPUT);
  digitalWrite(PIN_BTN_TX, LOW);
  delayMicroseconds(3);
  bool d3Low = digitalRead(PIN_BTN_D3) == LOW;

  pinMode(PIN_BTN_D3, INPUT_PULLUP);
  pinMode(PIN_BTN_TX, INPUT_PULLUP);

  if (txLow && d3Low) return BTN_PREVIOUS;
  if (txLow) return BTN_NEXT;
  if (d3Low) return BTN_PLAY_PAUSE;
  return BTN_NONE;
}

void handleButton(Button button) {
  if (button == BTN_PLAY_PAUSE) {
    if (mp3.isRunning()) {
      paused = !paused;  // The decoder and file position remain intact.
    } else {
      startTrack(trackIndex);
    }
  } else if (button == BTN_NEXT) {
    nextTrack();
  } else if (button == BTN_PREVIOUS) {
    previousTrack();
  }
  drawScreen();
}

void pollButtons() {
  static Button lastRaw = BTN_NONE;
  static Button stable = BTN_NONE;
  static uint32_t changedAt = 0;
  static uint32_t sampledAt = 0;

  if (millis() - sampledAt < 5) return;
  sampledAt = millis();
  Button raw = scanButtonMatrix();

  if (raw != lastRaw) {
    lastRaw = raw;
    changedAt = millis();
  }
  if (raw != stable && millis() - changedAt >= 35) {
    stable = raw;
    if (stable != BTN_NONE) handleButton(stable); // one action per press
  }
}

void updateVolume() {
  // Fixed volume: potentiometer/A0 control is intentionally disabled.
  i2s.SetGain(1.0f);  // 100% software gain
}

void printTrackName() {
  if (!trackCount) return;
  const char *name = tracks[trackIndex];
  if (name[0] == '/') ++name;

  char shown[20];
  strncpy(shown, name, sizeof(shown) - 1);
  shown[sizeof(shown) - 1] = '\0';
  char *extension = strrchr(shown, '.');
  if (extension) *extension = '\0';
  display.println(shown);
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("ESP-12E MP3 PLAYER"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(0, 16);

  if (fatalMessage) {
    display.println(fatalMessage);
  } else if (!trackCount) {
    display.println(F("No MP3 in SD root"));
  } else {
    display.print(trackIndex + 1);
    display.print('/');
    display.println(trackCount);
    printTrackName();
    display.println();
    display.println(paused ? F("PAUSED") : F("PLAYING"));
    display.println(F("Play  Next  Previous"));
  }
  display.display();
}

void setup() {
  // Do not use Serial: GPIO1/TX is part of the button matrix in this wiring.
  pinMode(PIN_BTN_D3, INPUT_PULLUP);
  pinMode(PIN_BTN_TX, INPUT_PULLUP);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  drawScreen();

  // SD uses the ESP8266 hardware-SPI pins shown in the schematic.
  sdReady = SD.begin(PIN_SD_CS);
  if (!sdReady) {
    fatalMessage = "SD card not found";
    drawScreen();
    return;
  }

  findTracks();
  if (!trackCount) {
    fatalMessage = "No MP3 in SD root";
    drawScreen();
    return;
  }

  updateVolume();
  startTrack(0);
}

void loop() {
  pollButtons();
  updateVolume();

  if (!paused && mp3.isRunning()) {
    if (!mp3.loop()) {
      stopTrack();
      nextTrack();
    }
  }

  static uint32_t lastRefresh = 0;
  if (millis() - lastRefresh >= 500) {
    lastRefresh = millis();
    drawScreen();
  }
  yield();
}
